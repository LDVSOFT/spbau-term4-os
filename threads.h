#pragma once

#include <stdint.h>
#include <stdbool.h>

static inline void barrier() {
	asm volatile ("" : : : "memory");
}

struct critical_section {
	volatile bool is_occupied;
};

void cs_init (struct critical_section* section);
void cs_finit(struct critical_section* section);
void cs_enter(struct critical_section* section);
void cs_leave(struct critical_section* section);

typedef void* (*thread_func_t)(void*);

#define THREAD_STACK_ORDER 1
#define THREAD_STACK_SIZE (PAGE_SIZE << THREAD_STACK_ORDER)

struct thread {
	struct critical_section* cs;

	const char *name;
	thread_func_t func;
	void* data;
	bool is_over;

	void* stack;
	void* stack_pointer;
	struct thread* prev;
	struct thread* next;
};

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

typedef void(*schedule_callback_t)(void);
void scheduler_init(void);
void schedule(schedule_callback_t callback, enum thread_new_state state);
void yield(void);

static inline void write_rflags(uint64_t rflags) {
	asm volatile ("push %0; popfq" : : "g"(rflags));
}

static inline uint64_t read_rflags(void) {
	uint64_t rflags;
	asm volatile ("pushfq; pop %0" : "=g"(rflags));
	return rflags;
}
