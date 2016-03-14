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
