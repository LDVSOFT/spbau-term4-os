#include "memory.h"
#include "bootstrap-alloc.h"
#include "paging.h"
#include "buddy.h"

struct paging_build_iterator {
	struct mmap_iterator super;
	phys_t max_memory;
};

static void __paging_build_iterator_iterate(struct paging_build_iterator* self, struct mmap_entry* entry) {
	phys_t end = entry->base_addr + entry->length;
	if (self->max_memory < end) {
		self->max_memory = end;
	}
}

static void paging_build_iterator_init(struct paging_build_iterator* self) {
	self->super.iterate = (mmap_iterator_iterate_t) __paging_build_iterator_iterate;
	self->max_memory = 0;
}

static void paging_build_region(virt_t start, virt_t length, phys_t base, pte_t pml4) {
	virt_t vpos = start;
	virt_t pos = base;
	while(1) {
		phys_t pml4e = pte_phys(pml4) + pml4_i(vpos);
		pte_t* pml4e_p = (pte_t*)va(pml4e);
		if (!pte_present(*pml4e_p)) {
			phys_t new_page = buddy_alloc(0);
			*pml4e_p = PTE_PRESENT | PTE_WRITE | new_page;
		}

		phys_t pdpte = pte_phys(*pml4e_p) + pml3_i(vpos);
		pte_t* pdpte_p = (pte_t*)va(pdpte);
		if (!pte_present(*pdpte_p)) {
			phys_t new_page = buddy_alloc(0);
			*pdpte_p = PTE_PRESENT | PTE_WRITE | new_page;
		}

		phys_t pde = pte_phys(*pdpte_p) + pml2_i(vpos);
		pte_t* pde_p = (pte_t*)va(pde);
		*pde_p = PTE_PRESENT | PTE_WRITE | PTE_LARGE | pos;

		vpos += BIG_PAGE_SIZE;
		pos += BIG_PAGE_SIZE;
		if (length < BIG_PAGE_SIZE) {
			break;
		}
		length -= BIG_PAGE_SIZE;
	}
}

void paging_build() {
	struct paging_build_iterator iterator;
	paging_build_iterator_init(&iterator);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);

	pte_t pml4 = buddy_alloc(0);
	paging_build_region(HIGH_BASE, iterator.max_memory, PHYSICAL_BASE, pml4);
	paging_build_region(KERNEL_BASE, KERNEL_SIZE, PHYSICAL_BASE, pml4);
	store_pml4(pml4);
}
