#include "bootstrap-alloc.h"
#include "buddy.h"
#include "halt.h"

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
	if (node_p >= buddy_allocator.nodes) {
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
	node_p->is_free = 0;
	node_p->prev = BUDDY_NODE_NULL;
	node_p->next = BUDDY_NODE_NULL;
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

struct buddy_init_iterator {
	struct mmap_iterator super;
	int isRealMode;
	uint64_t max_memory;
};

static void __buddy_init_iretate(struct buddy_init_iterator* self, struct mmap_entry* entry) {
	if (entry->type != MMAP_ENTRY_TYPE_AVAILABLE || entry->length == 0) {
		return;
	}
	uint64_t end = entry->base_addr + entry->length;
	if (! self->isRealMode) {
		if (self->max_memory < end) {
			self->max_memory = end;
		}
	} else {
		phys_t pos = entry->base_addr;
		pos = (pos + PAGE_SIZE - 1) % PAGE_SIZE; // Align start
		while (pos + PAGE_SIZE <= end) {
			buddy_free(buddy_node_from_address(pos));
			pos += PAGE_SIZE;
		}
	}
}

static void buddy_init_iterator_init(struct buddy_init_iterator* self) {
	self->super.iterate = (mmap_iterator_iterate_t)__buddy_init_iretate;
	self->isRealMode = 0;
	self->max_memory = 0;
}

void buddy_init() {
	bootstrap_init_mmap();

	struct buddy_init_iterator iterator;
	buddy_init_iterator_init(&iterator);
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);

	buddy_allocator.nodes_count = iterator.max_memory / PAGE_SIZE;
	buddy_allocator.nodes = (struct buddy_node*) bootstrap_alloc(buddy_allocator.nodes_count * sizeof(struct buddy_node));
	for (buddy_node_no i = BUDDY_NODE_START; i != buddy_allocator.nodes_count; ++i) {
		buddy_node_init(i);
	}

	// Now all nodes think they are aloocated pages. We need to deallocate the available ones.
	iterator.isRealMode = 1;
	mmap_iterate(bootstrap_mmap, bootstrap_mmap_length, (struct mmap_iterator*) &iterator);
}

phys_t buddy_alloc(int level) {
	int alloc_level = level;
	while (alloc_level < BUDDY_LEVELS && buddy_allocator.node_list_starts[alloc_level].next == BUDDY_NODE_NULL) {
		++alloc_level;
	}
	if (alloc_level == BUDDY_LEVELS) {
		return (phys_t)NULL;
	}

	buddy_node_no result = buddy_allocator.node_list_starts[alloc_level].next;
	struct buddy_node* result_p = buddy_node_from_no(result);
	result_p->is_free = 0;
	buddy_node_delete(result);
	while (result_p->level > level) {
		--result_p->level;
		buddy_node_no buddy = buddy_node_buddy(result);
		buddy_node_insert(buddy, buddy_node_to_no(&buddy_allocator.node_list_starts[result_p->level]));
	}
	return buddy_node_to_address(result);
}

void buddy_free(phys_t pointer) {
	buddy_node_no node = buddy_node_from_address(pointer);
	struct buddy_node* node_p = buddy_node_from_no(node);
	if (node_p->is_free) {
		return;
	}
	node_p->is_free = 1;
	buddy_node_insert(node, buddy_node_to_no(&buddy_allocator.node_list_starts[node_p->level]));
	while (node_p->level != BUDDY_LEVELS - 1) {
		buddy_node_no buddy = buddy_node_buddy(node);
		if (buddy == BUDDY_NODE_NULL) {
			break;
		}
		struct buddy_node* buddy_p = buddy_node_from_no(buddy);
		if (!buddy_p->is_free || buddy_p->level != node_p->level) {
			return;
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
