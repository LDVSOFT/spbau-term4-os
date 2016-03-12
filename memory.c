#include "memory.h"
#include "print.h"

extern const uint32_t mboot_info;

void mmap_iterate(void* mmap, uint32_t length, mmap_iterator iterator) {
	struct mmap_entry* entry = (struct mmap_entry*)mmap;
	struct mmap_entry* end   = (struct mmap_entry*)((virt_t)entry + length);
	while (entry < end) {
		iterator(entry);
		entry = (struct mmap_entry*)((virt_t)entry + sizeof(entry->size) + entry->size);
	}
}
