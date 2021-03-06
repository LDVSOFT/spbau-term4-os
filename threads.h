#pragma once

#define THREAD_STACK_ORDER 1
#define THREAD_STACK_SIZE 0x2000

#ifndef __ASM_FILE__

#include "list.h"
#include <stdint.h>
#include <stdbool.h>

static inline void barrier() {
	asm volatile ("" : : : "memory");
}

typedef void* (*thread_func_t)(void*);

struct mutex;

struct condition_variable {
	struct mutex* mutex;
	struct list_node threads_head;
};

struct mutex {
	bool is_occupied;
	struct condition_variable is_locked;
};

struct thread {
	struct mutex lock;
	struct condition_variable is_dead;

	const char *name;
	thread_func_t func;
	void* data;
	bool is_over;

	void* stack;
	void* stack_pointer;
	// Thread can be contained it 2 lists. Sheduler's one:
	struct list_node scheduler_link;
	// And another one (for condition variable, etc)
	struct list_node store_link;
};

void cv_init(struct condition_variable* varibale, struct mutex* mutex);
void cv_finit(struct condition_variable* variable);
void cv_wait(struct condition_variable* variable);
void cv_notify(struct condition_variable* variable);
void cv_notify_all(struct condition_variable* variable);

uint64_t hard_lock();
void hard_unlock(uint64_t rflags);

void mutex_init (struct mutex* mutex);
void mutex_finit(struct mutex* mutex);
void mutex_lock(struct mutex* mutex);
void mutex_unlock(struct mutex* mutex);

struct thread* thread_create(thread_func_t func, void* data, const char* name);
struct thread* thread_current(void);
void* thread_join(struct thread* thread);

// Assembly:
void thread_switch(void** old_stack, void* new_stack);
void thread_run_wrapper(void);

enum thread_new_state {
	THREAD_NEW_STATE_ALIVE,
	THREAD_NEW_STATE_SLEEP,
	THREAD_NEW_STATE_DEAD
};

void scheduler_init(void);
void schedule(enum thread_new_state state);
void yield(void);

static inline void write_rflags(uint64_t rflags) {
	asm volatile ("push %0; popfq" : : "g"(rflags));
}

static inline uint64_t read_rflags(void) {
	uint64_t rflags;
	asm volatile ("pushfq; pop %0" : "=g"(rflags));
	return rflags;
}

#endif /*__ASM_FILE__*/
