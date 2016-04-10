#pragma once

#include "memory.h"
#include "threads.h"
#include "list.h"
#include <stdint.h>

struct slab;
struct slab_node;
struct slab_allocator;

#define SLAB_SMALL (PAGE_SIZE / 8)

struct slab_node {
	struct list_node link;
	void* data;
} __attribute__((packed));

struct slab {
	struct list_node link;
	struct list_node nodes_head;
	phys_t page;
} __attribute__((packed));

struct slab_allocator {
	struct critical_section cs;
	struct list_node slabs_head;
	uint16_t obj_size;
	uint16_t obj_align;
};

void slab_allocators_init(void);

void slab_init(struct slab_allocator* allocator, uint16_t size, uint16_t align);
void slab_finit(struct slab_allocator* allocator);

void* slab_alloc(struct slab_allocator* allocator);
void slab_free(void* ptr);

#define slab_init_for(p, type) slab_init(p, sizeof(type), _Alignof(type))
