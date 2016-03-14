#include "memory.h"
#include "print.h"
#include "halt.h"

extern const uint32_t mboot_info;

void mmap_iterate(void* mmap, uint32_t length, struct mmap_iterator* iterator) {
	struct mmap_entry* entry = (struct mmap_entry*)mmap;
	struct mmap_entry* end   = (struct mmap_entry*)((virt_t)entry + length);
	while (entry < end) {
		iterator->iterate(iterator, entry);
		entry = (struct mmap_entry*)((virt_t)entry + sizeof(entry->size) + entry->size);
	}
}

// Bootstrap allocator's mmap
// Contains forbid-kernel region, also has actuall holes
// These holes is memory allocated by bootstrap allocator
#define BOOTSTRAP_MMAP_MAX_LENGTH 64

int bootstrap_mmap_length = 0;
struct mmap_entry bootstrap_mmap[BOOTSTRAP_MMAP_MAX_LENGTH];

extern char text_phys_begin[];
extern char bss_phys_end[];

static int __bootstrap_init_get() {
	if (bootstrap_mmap_length == BOOTSTRAP_MMAP_MAX_LENGTH) {
		halt("Bootstrap allocator: mmap buffer is depleted.\n");
	}
	return bootstrap_mmap_length++;
}

static void __bootstrap_init_mmap(struct mmap_iterator self, struct mmap_entry* entry) {
	if ((phys_t)bss_phys_end <= entry->base_addr || entry->base_addr + entry->length <= (phys_t)text_phys_begin) {
		struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
		*bootstrap_mmap_entry = *entry;
		bootstrap_mmap_entry->size = sizeof(struct mmap_entry);
	} else {
		if (entry->base_addr < (phys_t)text_phys_begin) { // Is something before kernel?
			struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
			bootstrap_mmap_entry->size = sizeof(struct mmap_entry);
			bootstrap_mmap_entry->base_addr = entry->base_addr;
			bootstrap_mmap_entry->length = (uint64_t)text_phys_begin - entry->base_addr;
			bootstrap_mmap_entry->type = entry->type;
		}
		// We will not add kernel region, because this allocator treats holes in mmap as allocated
		if ((phys_t)bss_phys_end < entry->base_addr + entry->length) { // Is something after kernel?
			struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[__bootstrap_init_get()];
			bootstrap_mmap_entry->size = sizeof(struct mmap_entry);
			bootstrap_mmap_entry->base_addr = (phys_t)bss_phys_end;
			bootstrap_mmap_entry->length = entry->base_addr + entry->length - (phys_t)bss_phys_end;
			bootstrap_mmap_entry->type = entry->type;
		}
	}
}

static struct mmap_iterator bootstrap_init_iterator = {(mmap_iterator_iterate_t)__bootstrap_init_mmap};

void bootstrap_init_mmap() {
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	if ((info->flags & (1 << MBOOT_INFO_MMAP)) == 0) {
		halt("No MMAP present.\n");
	}

	// Now copy all old regions, adding kernel one on the wat
	mmap_iterate(va(info->mmap_addr), info->mmap_length, &bootstrap_init_iterator);
}

struct bootstrap_alloc_iterator {
	struct mmap_iterator super;
	uint32_t size;
	void* result;
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
	self->result = va(entry->base_addr);
	entry->base_addr += self->size;
}

static void bootstrap_alloc_iterator_init(struct bootstrap_alloc_iterator* self,uint32_t size) {
	self->super.iterate = (mmap_iterator_iterate_t)__bootstrap_alloc;
	self->size = size;
	self->result = 0;
}

void* bootstrap_alloc(uint32_t size) {
	// We'll just go to our mmap
	// and cut something :)
	struct bootstrap_alloc_iterator iterator;
	bootstrap_alloc_iterator_init(&iterator, size);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length * sizeof(struct mmap_entry), (struct mmap_iterator*)&iterator);
	return iterator.result;
}
