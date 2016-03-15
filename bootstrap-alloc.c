#include "bootstrap-alloc.h"
#include "halt.h"
#include "multiboot.h"
#include "print.h"

// Bootstrap allocator's mmap
// Contains forbid-kernel region, also has actuall holes
// These holes is memory allocated by bootstrap allocator
int bootstrap_mmap_length = 0;
int bootstrap_mmap_size = 0;
struct mmap_entry bootstrap_mmap[BOOTSTRAP_MMAP_MAX_LENGTH];

extern char text_phys_begin[];
extern char bss_phys_end[];

static int __bootstrap_init_get() {
	if (bootstrap_mmap_size == BOOTSTRAP_MMAP_MAX_LENGTH) {
		halt("Bootstrap allocator: mmap buffer is depleted.\n");
	}
	bootstrap_mmap_length += sizeof(struct mmap_entry);
	bootstrap_mmap_size += 1;
	return bootstrap_mmap_size - 1;
}

static void __bootstrap_init_mmap(struct mmap_iterator self, struct mmap_entry* entry) {
	if ((phys_t)bss_phys_end <= entry->base_addr || entry->base_addr + entry->length <= (phys_t)text_phys_begin) {
		struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
		*bootstrap_mmap_entry = *entry;
		bootstrap_mmap_entry->size = MMAP_ENTRY_SIZE;
		if (bootstrap_mmap_entry->base_addr < BIOS_SIZE) {
			bootstrap_mmap_entry->type = MMAP_ENTRY_TYPE_RESERVED;
		}
	} else {
		if (entry->base_addr < (phys_t)text_phys_begin) { // Is something before kernel?
			struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
			bootstrap_mmap_entry->size = MMAP_ENTRY_SIZE;
			bootstrap_mmap_entry->base_addr = entry->base_addr;
			bootstrap_mmap_entry->length = (uint64_t)text_phys_begin - entry->base_addr;
			bootstrap_mmap_entry->type = entry->type;
			if (bootstrap_mmap_entry->base_addr < BIOS_SIZE) {
				bootstrap_mmap_entry->type = MMAP_ENTRY_TYPE_RESERVED;
			}
		}
		// We will not add kernel region, because this allocator treats holes in mmap as allocated
		if ((phys_t)bss_phys_end < entry->base_addr + entry->length) { // Is something after kernel?
			struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
			bootstrap_mmap_entry->size = MMAP_ENTRY_SIZE;
			bootstrap_mmap_entry->base_addr = (phys_t)bss_phys_end;
			bootstrap_mmap_entry->length = entry->base_addr + entry->length - (phys_t)bss_phys_end;
			bootstrap_mmap_entry->type = entry->type;
			if (bootstrap_mmap_entry->base_addr < BIOS_SIZE) {
				bootstrap_mmap_entry->type = MMAP_ENTRY_TYPE_RESERVED;
			}
		}
	}
}

static struct mmap_iterator bootstrap_init_iterator = {(mmap_iterator_iterate_t)__bootstrap_init_mmap};

void bootstrap_init_mmap() {
	printf("! bootstrap_init_mmap: kernel is [%p %p).\n", (phys_t)text_phys_begin, (phys_t)bss_phys_end);
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	if ((info->flags & (1 << MBOOT_INFO_MMAP)) == 0) {
		halt("No MMAP present.\n");
	}

	// Now copy all old regions, adding kernel one on the wat
	mmap_iterate(va(info->mmap_addr), info->mmap_length, &bootstrap_init_iterator);
	print_mmap(bootstrap_mmap, bootstrap_mmap_length);
}

struct bootstrap_alloc_iterator {
	struct mmap_iterator super;
	uint32_t size;
	phys_t result;
};

static void __bootstrap_alloc(struct bootstrap_alloc_iterator* self, struct mmap_entry* entry) {
	if (self->result) {
		return;
	}
	if (entry->type != MMAP_ENTRY_TYPE_AVAILABLE) {
		return;
	}
	if (entry->length < self->size) {
		return;
	}
	printf("! __bootstrap_alloc: entry %p\n", entry->base_addr);
	self->result = entry->base_addr;
	entry->length -= self->size;
	entry->base_addr += self->size;
}

static void bootstrap_alloc_iterator_init(struct bootstrap_alloc_iterator* self,uint32_t size) {
	self->super.iterate = (mmap_iterator_iterate_t)__bootstrap_alloc;
	self->size = size;
	self->result = 0;
}

phys_t bootstrap_alloc(uint32_t size) {
	// We'll just go to our mmap
	// and cut something :)
	printf("! bootstrap_alloc: %u bytes.\n", size);
	struct bootstrap_alloc_iterator iterator;
	bootstrap_alloc_iterator_init(&iterator, size);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*)&iterator);
	return iterator.result;
}
