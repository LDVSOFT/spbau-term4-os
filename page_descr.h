#pragma once

#include "slab-allocator.h"
#include <stddef.h>

struct page_descr {
	struct slab_allocator* slab_allocator;
};

static inline void page_descr_init(struct page_descr* page_descr) {
	page_descr->slab_allocator = NULL;
}
