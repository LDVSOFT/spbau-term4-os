#pragma once

#define THREAD_STACK_ORDER 1
#define THREAD_STACK_SIZE 0x2000

#ifndef __ASM_FILE__

#include <stdint.h>
#include <stdbool.h>

static inline void barrier() {
	asm volatile ("" : : : "memory");
}

typedef void* (*thread_func_t)(void*);

struct critical_section;
struct condition_variable;

struct thread {
	volatile struct critical_section* cs;
	volatile struct condition_variable* is_dead;

	const char *name;
	thread_func_t func;
	volatile void* data;
	volatile bool is_over;

	void* stack;
	void* stack_pointer;
	// Thread can be contained it 2 lists. Sheduler's one:
	volatile struct thread* prev;
	volatile struct thread* next;
	// And another one (for condition variable, etc)
	volatile struct thread* alt_prev;
	volatile struct thread* alt_next;
};

struct condition_variable {
	volatile struct critical_section* section;
	struct thread head;
};

struct critical_section {
	volatile bool is_occupied;
	struct condition_variable is_locked;
};

void cv_init(volatile struct condition_variable* varibale, volatile struct critical_section* section);
void cv_finit(volatile struct condition_variable* variable);
void cv_wait(volatile struct condition_variable* variable);
void cv_notify(volatile struct condition_variable* variable);
void cv_notify_all(volatile struct condition_variable* variable);

void cs_init (volatile struct critical_section* section);
void cs_finit(volatile struct critical_section* section);
void cs_enter(volatile struct critical_section* section);
void cs_leave(volatile struct critical_section* section);

struct thread* thread_create(thread_func_t func, void* data, const char* name);
volatile struct thread* thread_current(void);
void* thread_join(struct thread* thread);

// Assembly:
void thread_switch(void* volatile* old_stack, volatile void* new_stack);
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
