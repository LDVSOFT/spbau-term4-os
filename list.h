#pragma once

#include <stdbool.h>
#include <stddef.h>

struct list_node {
	struct list_node* prev;
	struct list_node* next;
};

bool list_empty(struct list_node* head);
struct list_node* list_first(struct list_node* head);

void list_init(struct list_node* node);
void list_add(struct list_node* node, struct list_node* head);
void list_add_tail(struct list_node* node, struct list_node* head);
void list_delete(struct list_node* node);

#define LIST_ENTRY(node, type, member) ( (type*) ((char*)(node) - offsetof(type, member)) )
