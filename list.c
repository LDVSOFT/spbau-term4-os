#include "list.h"

bool list_empty(struct list_node* head) {
	return head->next == head;
}

struct list_node* list_first(struct list_node* head) {
	return head->next;
}

void list_init(struct list_node* node) {
	node->prev = node->next = node;
}

static void list_insert(struct list_node* node, struct list_node* prev, struct list_node* next) {
	node->next = next;
	node->prev = prev;
	next->prev = node;
	prev->next = node;
}

void list_add(struct list_node* node, struct list_node* head) {
	list_insert(node, head, head->next);
}

void list_add_tail(struct list_node* node, struct list_node* head) {
	list_insert(node, head->prev, head);
}

static void list_remove(struct list_node* node, struct list_node* prev, struct list_node* next) {
	prev->next = next;
	next->prev = prev;
	list_init(node);
}

void list_delete(struct list_node* node) {
	list_remove(node, node->prev, node->next);
}
