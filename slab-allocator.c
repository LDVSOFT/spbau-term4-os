#include "slab-allocator.h"
#include "buddy.h"
#include "log.h"

// Small slab

static struct slab* slab_small_new(struct slab_allocator* main, uint16_t size, uint16_t align) {
	phys_t page = buddy_alloc(0);
	if (page == (phys_t)NULL) {
		return NULL;
	}
	struct slab* slab = (struct slab*)( (virt_t)va(page) + PAGE_SIZE - sizeof(struct slab) );
	list_init(&slab->link);
	list_init(&slab->nodes_head);
	slab->page = page;

	virt_t block_size = sizeof(struct slab_node) + size;
	virt_t shift = ((block_size + align - 1) / align) * align;

	for (virt_t pos = (virt_t)va(page); pos + shift <= (virt_t)slab; pos += shift) {
		struct slab_node* node = (struct slab_node*) pos;
		node->data = (void*)( pos + sizeof(struct slab_node) );
		list_add(&node->link, &slab->nodes_head);
	}
	page_descr_for(page)->slab_allocator = main;
	log(LEVEL_V, "New small slab created at page %p for size=%hu, align=%hu.", page, size, align);
	return slab;
}

static void slab_small_delete(struct slab* slab) {
	log(LEVEL_V, "Small slab deleted from page %p.", slab->page);
	page_descr_for(slab->page)->slab_allocator = NULL;
	buddy_free(slab->page);
}

static void* slab_small_alloc(struct slab* slab) {
	if (list_empty(&slab->nodes_head)) {
		return NULL;
	}
	struct slab_node* node = LIST_ENTRY(list_first(&slab->nodes_head), struct slab_node, link);
	void* ptr = node->data;
	list_delete(&node->link);
	return ptr;
}

static void slab_small_free(struct slab* slab, void* ptr) {
	struct slab_node* node = (struct slab_node*)( (virt_t)ptr - sizeof(struct slab_node) );
	list_add(&node->link, &slab->nodes_head);
}

// Big

static struct slab_allocator big_slab_struct_allocator;
static struct slab_allocator big_slab_node_allocator;

static void slab_big_delete(struct slab* slab);

static struct slab* slab_big_new(struct slab_allocator* main, uint16_t size, uint16_t align) {
	struct slab* slab = (struct slab*) slab_alloc(&big_slab_struct_allocator);
	if (slab == NULL) {
		return NULL;
	}
	phys_t page = buddy_alloc(0);
	if (page == (phys_t) NULL) {
		slab_free(slab);
		return NULL;
	}

	list_init(&slab->link);
	list_init(&slab->nodes_head);
	slab->page = page;

	virt_t shift = ((size + align - 1) / align) * align;
	for (virt_t pos = (virt_t)va(page); pos + shift <= (virt_t)va(page) + PAGE_SIZE; pos += shift) {
		struct slab_node* node = (struct slab_node*)slab_alloc(&big_slab_node_allocator);
		if (node == NULL) {
			slab_big_delete(slab);
			return NULL;
		}
		node->data = (void*)pos;
		list_add(&node->link, &slab->nodes_head);
	}
	page_descr_for(page)->slab_allocator = main;
	log(LEVEL_V, "New big slab created at page %p for size=%hu, align=%hu.", page, size, align);
	return slab;
}

static void slab_big_delete(struct slab* slab) {
	page_descr_for(slab->page)->slab_allocator = NULL;
	for (struct list_node* list_node = list_first(&slab->nodes_head); list_node != &slab->nodes_head; ) {
		struct slab_node* node = LIST_ENTRY(list_node, struct slab_node, link);
		list_node = list_node->next;
		slab_free(node);
	}
	log(LEVEL_V, "Big slab deleted from page %p.", slab->page);
	buddy_free(slab->page);
}

static void* slab_big_alloc(struct slab* slab) {
	if (list_empty(&slab->nodes_head)) {
		return NULL;
	}
	struct slab_node* node = LIST_ENTRY(&slab->nodes_head, struct slab_node, link);
	void* ptr = node->data;
	list_delete(&node->link);
	slab_free(node);
	return ptr;
}

static void slab_big_free(struct slab* slab, void* ptr) {
	struct slab_node* node = (struct slab_node*)slab_alloc(&big_slab_node_allocator);
	if (node == NULL) {
		halt("No memory for new slab node to free memory!");
	}
	node->data = ptr;
	list_add(&slab->nodes_head, &node->link);
}

// Allocator

void slab_allocators_init(void) {
	slab_init(&big_slab_struct_allocator, sizeof(struct slab)     , 1);
	slab_init(&big_slab_node_allocator  , sizeof(struct slab_node), 1);
}

void slab_init(struct slab_allocator* slab_allocator, uint16_t size, uint16_t align) {
	cs_init(&slab_allocator->cs);
	slab_allocator->obj_size = size;
	slab_allocator->obj_align = align;
	struct slab* slab;
	if (size <= SLAB_SMALL) {
		slab = slab_small_new(slab_allocator, size, align);
	} else {
		slab = slab_big_new(slab_allocator, size, align);
	}
	list_add(&slab_allocator->slabs_head, &slab->link);
}

void slab_finit(struct slab_allocator* slab_allocator) {
	for (struct list_node* list_node = list_first(&slab_allocator->slabs_head); list_node != &slab_allocator->slabs_head; ) {
		struct slab* slab = LIST_ENTRY(list_node, struct slab, link);
		list_node = list_node->next;
		if (slab_allocator->obj_size <= SLAB_SMALL) {
			slab_small_delete(slab);
		} else {
			slab_big_delete(slab);
		}
	}
	cs_finit(&slab_allocator->cs);
}

static void* __slab_alloc(struct slab_allocator* slab_allocator) {
	struct slab* slab;
	for (
			struct list_node* list_node = list_first(&slab_allocator->slabs_head);
			list_node != &slab_allocator->slabs_head;
			list_node = list_node->next
	) {
		slab = LIST_ENTRY(list_node, struct slab, link);
		void* ptr;
		if (slab_allocator->obj_size <= SLAB_SMALL) {
			ptr = slab_small_alloc(slab);
		} else {
			ptr = slab_big_alloc(slab);
		}
		if (ptr) {
			return ptr;
		}
	}
	// At this point, there is no free slab
	if (slab_allocator->obj_size <= SLAB_SMALL) {
		slab = slab_small_new(slab_allocator, slab_allocator->obj_size, slab_allocator->obj_align);
		if (slab == NULL) {
			return NULL;
		}
		list_add(&slab->link, &slab_allocator->slabs_head);
		return slab_small_alloc(slab);
	} else {
		slab = slab_big_new(slab_allocator, slab_allocator->obj_size, slab_allocator->obj_align);
		if (slab == NULL) {
			return NULL;
		}
		list_add(&slab->link, &slab_allocator->slabs_head);
		return slab_big_alloc(slab);
	}
}

void* slab_alloc(struct slab_allocator* slab_allocator) {
	cs_enter(&slab_allocator->cs);
	void* res = __slab_alloc(slab_allocator);
	cs_leave(&slab_allocator->cs);
	return res;
}

void slab_free(void* ptr) {
	if (ptr == NULL) {
		return;
	}
	phys_t ptr_phys = pa(ptr);
	struct slab_allocator* slab_allocator = page_descr_for(ptr_phys)->slab_allocator;
	cs_enter(&slab_allocator->cs);
	for (
			struct list_node* list_node = list_first(&slab_allocator->slabs_head);
			list_node != &slab_allocator->slabs_head;
			list_node = list_node->next
	) {
		struct slab* slab = LIST_ENTRY(list_node, struct slab, link);
		if (slab->page <= ptr_phys && ptr_phys < slab->page + PAGE_SIZE) {
			if (slab_allocator->obj_size <= SLAB_SMALL) {
				slab_small_free(slab, ptr);
			} else {
				slab_big_free(slab, ptr);
			}
			break;
		}
	}
	cs_leave(&slab_allocator->cs);
}
