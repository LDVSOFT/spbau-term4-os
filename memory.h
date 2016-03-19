#pragma once

#define PAGE_BITS         12
#define PAGE_SIZE         (1 << 12)
#define PAGE_MASK         (PAGE_SIZE - 1)
#define KERNEL_BASE       0xffffffff80000000
#define HIGH_BASE         0xffff800000000000
#define PHYSICAL_BASE     0x0000000000000000

#define KERNEL_SIZE       (2ull * 1024ull * 1024ull * 1024ull)
#define BIOS_SIZE         (1ull * 1024ull * 1024ull)

#define BIG_PAGE_SIZE     1 << 21

#define KERNEL_CODE       0x18
#define KERNEL_DATA       0x20

#define KERNEL_PHYS(x)    ((x) - KERNEL_BASE)
#define KERNEL_VIRT(x)    ((x) + KERNEL_BASE)
#define PA(x)             ((x) - HIGH_BASE)
#define VA(x)             ((x) + HIGH_BASE)

#ifndef __ASM_FILE__

#include "multiboot.h"
#include <stdint.h>

#define BOOTMEM_SIZE      (4ull * 1024ull * 1024ull * 1024ull)

typedef uintptr_t phys_t;
typedef uintptr_t virt_t;

static inline uintptr_t kernel_phys(void *addr)
{ return KERNEL_PHYS((uintptr_t)addr); }

static inline void *kernel_virt(uintptr_t addr)
{ return (void *)KERNEL_VIRT(addr); }

static inline phys_t pa(const void *addr)
{ return PA((virt_t)addr); }

static inline void *va(phys_t addr)
{ return (void *)VA(addr); }

static inline uintmax_t get_bits(uintmax_t data, int start_bit, int count) {
	return (data >> start_bit) & ((UINTMAX_C(1) << count) - 1);
}

struct mmap_entry {
	uint32_t size;
	uint64_t base_addr;
	uint64_t length;
	uint32_t type;
} __attribute__((packed));
#define MMAP_ENTRY_SIZE (sizeof(struct mmap_entry) - sizeof(uint32_t))

#define MMAP_ENTRY_TYPE_RESERVED  0
#define MMAP_ENTRY_TYPE_AVAILABLE 1

struct mmap_iterator;
typedef void (*mmap_iterator_iterate_t)(struct mmap_iterator* self, struct mmap_entry* entry);
struct mmap_iterator {
	mmap_iterator_iterate_t iterate;
};

void mmap_iterate(void* mmap, uint32_t length, struct mmap_iterator *iterator);
void print_mmap(void* mmap, uint32_t length);

#endif /*__ASM_FILE__*/
