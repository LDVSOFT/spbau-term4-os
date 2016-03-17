#pragma once

#include "memory.h"
#include <stdint.h>

struct slab;
struct slab_node;
struct slab_allocator;

#define SLAB_SMALL (PAGE_SIZE / 8)

struct slab_node {
	struct slab_node* next;
	void* data;
};

struct slab {
	struct slab* next;
	struct slab_node* head;
	phys_t page;
};

struct slab_allocator {
	struct slab* head;
	uint16_t obj_size;
	uint16_t obj_align;
};

void slab_allocators_init();

void slab_init(struct slab_allocator* allocator, uint16_t size, uint16_t align);
void slab_finit(struct slab_allocator* allocator);

void* slab_alloc(struct slab_allocator* allocator);
void slab_free(void* ptr);
