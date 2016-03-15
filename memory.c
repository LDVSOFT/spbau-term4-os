#include "memory.h"
#include "print.h"

void mmap_iterate(void* mmap, uint32_t length, struct mmap_iterator* iterator) {
	struct mmap_entry* entry = (struct mmap_entry*)mmap;
	struct mmap_entry* end   = (struct mmap_entry*)((virt_t)entry + length);
	while (entry < end) {
		iterator->iterate(iterator, entry);
		entry = (struct mmap_entry*)((virt_t)entry + sizeof(entry->size) + entry->size);
	}
}

struct print_mmap_iterator {
	struct mmap_iterator super;
	phys_t last;
};

static void __print_mmap(struct print_mmap_iterator* self, struct mmap_entry* entry) {
	if (self->last != entry->base_addr) {
		printf("[%p .. %p) HOLE\n", self->last, entry->base_addr);
	}
	phys_t end = entry->base_addr + entry->length;
	printf("[%p .. %p) %s\n", entry->base_addr, end, entry->type == MMAP_ENTRY_TYPE_AVAILABLE ? "Available" : "Reserved");
	self->last = end;
}

static void print_mmap_iterator_init(struct print_mmap_iterator* self) {
	self->super.iterate = (mmap_iterator_iterate_t)__print_mmap;
	self->last = 0;
}

void print_mmap(void* mmap, uint32_t length) {
	printf("MMAP at %p, length = %u:\n", mmap, length);
	struct print_mmap_iterator iterator;
	print_mmap_iterator_init(&iterator);
	mmap_iterate(mmap, length, (struct mmap_iterator*)&iterator);
}
