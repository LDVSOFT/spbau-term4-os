#pragma once

#include "memory.h"
#include "page_descr.h"
#include "threads.h"
#include <stddef.h>

#define BUDDY_LEVELS 21
#define BUDDY_MAX_NODE_LENGTH (PAGE_LENGTH * (1 << (BUDDY_LEVELS - 1)))

// Order:
// -BUDDY_LEVELS        -- NULL
// -BUDDY_LEVELS+1 - -1 -- LIST STARTS
//               0 -    -- real nodes
#define BUDDY_NODE_NULL  (-BUDDY_LEVELS)
#define BUDDY_NODE_START (-BUDDY_LEVELS+1)
typedef int32_t buddy_node_no;

struct buddy_node {
	uint8_t level:7;
	uint8_t is_free:1;
	buddy_node_no prev;
	buddy_node_no next;
	struct page_descr page_descr;
};

struct buddy_allocator {
	struct mutex lock;
	struct buddy_node node_list_starts[BUDDY_LEVELS];
	buddy_node_no nodes_count;
	struct buddy_node* nodes;
};

void buddy_init(void);
void buddy_init_high(void);
phys_t buddy_alloc(int level);
void buddy_free(phys_t pointer);

struct page_descr* page_descr_for(phys_t ptr);

static inline uint64_t buddy_size(int level) {
	return 1ull << (level + PAGE_BITS);
}
