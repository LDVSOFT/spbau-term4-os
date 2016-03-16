#include "memory.h"
#include "bootstrap-alloc.h"
#include "paging.h"
#include "buddy.h"
#include "log.h"

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

static phys_t paging_new_page() {
	phys_t res = buddy_alloc(0);
	pte_t* res_p = (pte_t*)va(res);
	for (int i = 0; i != 512; ++i) {
		*(res_p + i) = 0ull;
	}
	return res;
}

static void paging_build_region(virt_t start, virt_t length, phys_t base, pte_t pml4) {
	log(LEVEL_V, "From %p to %p, at %p. PML4 is at %p.", start, start + length, base, pte_phys(pml4));
	virt_t vpos = start;
	virt_t pos = base;
	while(1) {
		pte_t* pml4e_p = (pte_t*)va(pte_phys(pml4)) + pml4_i(vpos);
		if (!pte_present(*pml4e_p)) {
			phys_t new_page = paging_new_page();
			*pml4e_p = PTE_PRESENT | PTE_WRITE | new_page;
		}

		pte_t* pdpte_p = (pte_t*)va(pte_phys(*pml4e_p)) + pml3_i(vpos);
		if (!pte_present(*pdpte_p)) {
			phys_t new_page = paging_new_page();
			*pdpte_p = PTE_PRESENT | PTE_WRITE | new_page;
		}

		pte_t* pde_p = (pte_t*)va(pte_phys(*pdpte_p)) + pml2_i(vpos);
		*pde_p = PTE_PRESENT | PTE_WRITE | PTE_LARGE | pos;

		vpos += BIG_PAGE_SIZE;
		pos += BIG_PAGE_SIZE;
		if (length <= BIG_PAGE_SIZE) {
			break;
		}
		length -= BIG_PAGE_SIZE;
	}
}

void paging_build() {
	struct paging_build_iterator iterator;
	paging_build_iterator_init(&iterator);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
	log(LEVEL_INFO, "Build paging, phys max is %p.", iterator.max_memory);
	pte_t pml4 = paging_new_page();
	paging_build_region(HIGH_BASE, iterator.max_memory, PHYSICAL_BASE, pml4);
	paging_build_region(KERNEL_BASE, KERNEL_SIZE, PHYSICAL_BASE, pml4);
	print_paging(pml4);
	store_pml4(pml4);
	flush_tlb();
	log(LEVEL_INFO, "Paging upgraded!");
}

void print_paging(pte_t pml4) {
	log(LEVEL_VVV, "CR3: PML4 at %p.", pte_phys(pml4));
	for (int i4 = 0; i4 != 512; ++i4) {
		pte_t pml4e = *( (pte_t*)va(pte_phys(pml4)) + i4 );
		if (!pte_present(pml4e)) {
			continue;
		}
		log(LEVEL_VVV, "  PML4E %3d: PDPT at %p, user=%d, write=%d.", i4, pte_phys(pml4e), pte_user(pml4e), pte_write(pml4e));
		for (int i3 = 0; i3 != 512; ++i3) {
			pte_t pdpte = *( (pte_t*)va(pte_phys(pml4e)) + i3 );
			if (!pte_present(pdpte)) {
				continue;
			}
			if (pte_large(pdpte)) {
				log(LEVEL_VVV, "    PDPTE %3d: 1GB page %p -> %p, user=%d, write=%d.", i3, pml_build(i4, i3, 0, 0, 0), pte_phys_large(pdpte), pte_user(pdpte), pte_write(pdpte));
				continue;
			}
			log(LEVEL_VVV, "    PDPTE %3d: PD at %p, user=%d, write=%d.", i3, pte_phys(pdpte), pte_user(pdpte), pte_write(pdpte));
			for (int i2 = 0; i2 != 512; ++i2) {
				pte_t pde = *( (pte_t*)va(pte_phys(pdpte)) + i2 );
				if (!pte_present(pde)) {
					continue;
				}
				if (pte_large(pde)) {
					log(LEVEL_VVV, "      PDE %3d: 2MB page %p -> %p, user=%d, write=%d.", i2, pml_build(i4, i3, i2, 0, 0), pte_phys_big(pde), pte_user(pde), pte_write(pde));
					continue;
				}
				log(LEVEL_VVV, "      PDE %3d: PT at %p, user=%d, write=%d.", i2, pte_phys(pde), pte_user(pde), pte_write(pde));
				for (int i1 = 0; i1 != 512; ++i2) {
					pte_t pte = *( (pte_t*)va(pte_phys(pde)) + i1 );
					if (!pte_present(pte)) {
						continue;
					}
					log(LEVEL_VVV, "        PTE %3d: 4KB page %p -> %p, user=%d, write=%d.", i1, pml_build(i4, i3, i2, i1, 0), pte_phys(pte), pte_user(pte), pte_write(pte));
				}
			}
		}
	}
}
