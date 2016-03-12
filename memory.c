#include "memory.h"
#include "print.h"

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
static int bootstrap_mmap_length = 0;
static struct mmap_entry bootstrap_mmap[32];

static void bootstrap_copy_mmap_entry(struct mmap_iterator self, struct mmap_entry* entry) {
	struct mmap_entry* bootstrap_mmap_entry = &bootstrap_mmap[bootstrap_mmap_length];
	*bootstrap_mmap_entry = *entry;
	bootstrap_mmap->size = sizeof(struct mmap_entry);
}

static struct mmap_iterator bootstrap_copy_iterator = {(mmap_iterator_iterate_t)bootstrap_copy_mmap_entry};

extern char text_phys_begin[];
extern char bss_phys_end[];

void bootstrap_init_mmap() {
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	if ((info->flags & (1 << MBOOT_INFO_MMAP)) == 0) {
		printf("ERROR: No MMAP present, hanging up!");
		while (1) ;
	}

	// Add kernel region to mmap first.
	// After that, any hit will see this region occupied,
	// so the entry later that it's occupied will not be taken into account.
	// Hacky but nice
	struct mmap_entry* entry = &bootstrap_mmap[bootstrap_mmap_length++];
	entry->size = sizeof(struct mmap_entry);
	entry->base_addr = (phys_t)text_phys_begin;
	entry->length = ((phys_t)bss_phys_end) - ((phys_t)text_phys_begin);
	entry->type = 0;

	// Now copy all old regions
	mmap_iterate(va(info->mmap_addr), info->mmap_length, &bootstrap_copy_iterator);
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
