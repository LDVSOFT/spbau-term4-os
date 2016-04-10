#include "bootstrap-alloc.h"
#include "memory.h"
#include "buddy.h"
#include "log.h"
#include <stdbool.h>

struct buddy_allocator buddy_allocator;

// Convert buddy node number to/from pointer to node
static inline struct buddy_node* buddy_node_from_no(buddy_node_no number) {
	if (number >= 0) {
		return &buddy_allocator.nodes[number];
	} else {
		return &buddy_allocator.node_list_starts[number + BUDDY_LEVELS - 1];
	}
}

static inline buddy_node_no buddy_node_to_no(struct buddy_node* node_p) {
	if (node_p >= buddy_allocator.nodes && node_p < buddy_allocator.nodes + buddy_allocator.nodes_count) {
		return node_p - buddy_allocator.nodes;
	} else {
		return node_p - buddy_allocator.node_list_starts - BUDDY_LEVELS + 1;
	}
}

// Get buddy number
static inline buddy_node_no buddy_node_buddy(buddy_node_no node) {
	if (node < 0) {
		return BUDDY_NODE_NULL;
	} else {
		return node ^ (1 << buddy_node_from_no(node)->level);
	}
}

// Initialise buddy node
static void buddy_node_init(buddy_node_no node) {
	struct buddy_node* node_p = buddy_node_from_no(node);
	node_p->level = 0;
	node_p->is_free = false;
	node_p->prev = BUDDY_NODE_NULL;
	node_p->next = BUDDY_NODE_NULL;
	page_descr_init(&node_p->page_descr);
}

// Convert buddy node number to/from page adress
static inline buddy_node_no buddy_node_from_address(phys_t adress) {
	return adress / PAGE_SIZE;
}

static inline phys_t buddy_node_to_address(buddy_node_no no) {
	return (phys_t)no * PAGE_SIZE;
}

// Insert node to list
static void buddy_node_insert(buddy_node_no node, buddy_node_no before) {
	struct buddy_node* node_p = buddy_node_from_no(node);
	struct buddy_node* before_p = buddy_node_from_no(before);
	node_p->next = before_p->next;
	node_p->prev = before;

	before_p->next = node;
	if (node_p->next != BUDDY_NODE_NULL) {
		buddy_node_from_no(node_p->next)->prev = node;
	}
}

// Delte node from list
static void buddy_node_delete(buddy_node_no node) {
	struct buddy_node* node_p = buddy_node_from_no(node);

	buddy_node_no prev = node_p->prev;
	buddy_node_no next = node_p->next;
	if (next) {
		buddy_node_from_no(next)->prev = prev;
	}
	if (prev) {
		buddy_node_from_no(prev)->next = next;
	}
	node_p->prev = node_p->next = BUDDY_NODE_NULL;
}

enum buddy_init_iterator_mode {
		MODE_CALC,
		MODE_INIT_LOW,
		MODE_INIT_HIGH
};

struct buddy_init_iterator {
	struct mmap_iterator super;
	enum buddy_init_iterator_mode mode;
	uint64_t max_memory;
};

static void __buddy_init_iterate(struct buddy_init_iterator* self, struct mmap_entry* entry) {
	if (entry->type != MMAP_ENTRY_TYPE_AVAILABLE || entry->length == 0) {
		return;
	}
	phys_t end = entry->base_addr + entry->length;
	phys_t pos;
	switch (self->mode) {
		case MODE_CALC:
			if (self->max_memory < end) {
				self->max_memory = end;
			}
			break;
		case MODE_INIT_LOW:
		case MODE_INIT_HIGH:
			pos = entry->base_addr;
			pos = ((pos + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE; // Align start
			if (self->mode == MODE_INIT_HIGH && pos < BOOTMEM_SIZE) {
				pos = BOOTMEM_SIZE;
			}
			while (pos + PAGE_SIZE <= end) {
				if (self->mode == MODE_INIT_LOW && pos >= BOOTMEM_SIZE) {
					break;
				}
				buddy_free(pos);
				pos += PAGE_SIZE;
			}
			break;
	}
}

static void buddy_init_iterator_init(struct buddy_init_iterator* self) {
	self->super.iterate = (mmap_iterator_iterate_t)__buddy_init_iterate;
	self->mode = MODE_CALC;
	self->max_memory = 0;
}

void buddy_init(void) {
	bootstrap_init_mmap();

	mutex_init(&buddy_allocator.lock);
	struct buddy_init_iterator iterator;
	buddy_init_iterator_init(&iterator);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
	buddy_allocator.nodes_count = iterator.max_memory / PAGE_SIZE;
	log(LEVEL_INFO, "There is %p memory, will need %d nodes.", iterator.max_memory, buddy_allocator.nodes_count);
	buddy_allocator.nodes = (struct buddy_node*)va(bootstrap_alloc(buddy_allocator.nodes_count * sizeof(struct buddy_node)));
	log(LEVEL_LOG, "Allocated at %p.", buddy_allocator.nodes);
	for (buddy_node_no i = BUDDY_NODE_START; i != buddy_allocator.nodes_count; ++i) {
		buddy_node_init(i);
	}
	log(LEVEL_V, "Nodes init cycle completed.");

	// Now all nodes think they are aloocated pages. We need to deallocate the available ones.
	iterator.mode = MODE_INIT_LOW;
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
	log(LEVEL_LOG, "Low memory freed.");
}

void buddy_init_high(void) {
	struct buddy_init_iterator iterator;
	buddy_init_iterator_init(&iterator);
	iterator.mode = MODE_INIT_HIGH;
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
	log(LEVEL_LOG, "High memory freed.");
}

static phys_t __buddy_alloc(int level) {
	int alloc_level = level;
	while (alloc_level < BUDDY_LEVELS && buddy_allocator.node_list_starts[alloc_level].next == BUDDY_NODE_NULL) {
		++alloc_level;
	}
	if (alloc_level == BUDDY_LEVELS) {
		return (phys_t)NULL;
	}

	buddy_node_no result = buddy_allocator.node_list_starts[alloc_level].next;
	struct buddy_node* result_p = buddy_node_from_no(result);
	result_p->is_free = false;
	buddy_node_delete(result);
	while (result_p->level > level) {
		--result_p->level;
		buddy_node_no buddy = buddy_node_buddy(result);
		buddy_node_insert(buddy, buddy_node_to_no(&buddy_allocator.node_list_starts[result_p->level]));
	}
	return buddy_node_to_address(result);
}

phys_t buddy_alloc(int level) {
	mutex_lock(&buddy_allocator.lock);
	phys_t res = __buddy_alloc(level);
	mutex_unlock(&buddy_allocator.lock);
	return res;
}

static void __buddy_free(phys_t pointer) {
	buddy_node_no node = buddy_node_from_address(pointer);
	struct buddy_node* node_p = buddy_node_from_no(node);
	if (node_p->is_free) {
		return;
	}
	node_p->is_free = true;
	buddy_node_insert(node, buddy_node_to_no(&buddy_allocator.node_list_starts[node_p->level]));
	while (node_p->level != BUDDY_LEVELS - 1) {
		buddy_node_no buddy = buddy_node_buddy(node);
		if (buddy == BUDDY_NODE_NULL) {
			break;
		}
		struct buddy_node* buddy_p = buddy_node_from_no(buddy);
		if (!buddy_p->is_free || buddy_p->level != node_p->level) {
			break;
		}

		// Merge
		buddy_node_no result        = (node < buddy) ? node : buddy;
		struct buddy_node* result_p = (node < buddy) ? node_p : buddy_p;

		result_p->level++;
		buddy_node_delete(node);
		buddy_node_delete(buddy);
		buddy_node_insert(result, buddy_node_to_no(&buddy_allocator.node_list_starts[result_p->level]));

		node   = result;
		node_p = result_p;
	}
}

void buddy_free(phys_t ptr) {
	mutex_lock(&buddy_allocator.lock);
	__buddy_free(ptr);
	mutex_unlock(&buddy_allocator.lock);
}

struct page_descr* page_descr_for(phys_t ptr) {
	return &((struct buddy_node*)buddy_node_from_no(buddy_node_from_address(ptr)))->page_descr;
}
