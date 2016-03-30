#include "threads.h"
#include "interrupt.h"
#include "memory.h"
#include "buddy.h"
#include "slab-allocator.h"
#include "print.h"
#include "log.h"

// It's 2016
// We don't care about SMP $)
// Either we care about sheduling done right

static volatile bool is_mutlithread = false;

void cs_init(struct critical_section* section) {
	section->is_occupied = false;
}

void cs_finit(struct critical_section* section) {
}

void cs_enter(struct critical_section* section) {
	if (!is_mutlithread) {
		return;
	}
	interrupt_disable();
	barrier();
	while (section->is_occupied) {
		schedule(NULL, THREAD_NEW_STATE_ALIVE);
		barrier();
	}
	section->is_occupied = true;
	barrier();
	interrupt_enable();
}

void cs_leave(struct critical_section* section) {
	if (!is_mutlithread) {
		return;
	}
	interrupt_disable();
	section->is_occupied = false;
	barrier();
	interrupt_enable();
}

static struct slab_allocator thread_allocator;

// There always must be at least one alive thread (idle for example)
static struct {
	struct critical_section cs;

	schedule_callback_t callback;
	struct thread* current;
	struct thread* alive;
	struct thread* alive_tail;
	struct thread* sleep;
	struct thread* dead;
} scheduler;

static void thread_delete(struct thread* thread) {
	if (scheduler.alive_tail == thread) {
		scheduler.alive_tail = thread->prev;
	}
	if (thread->prev != NULL) {
		thread->prev->next = thread->next;
	}
	if (thread->next != NULL) {
		thread->next->prev = thread->prev;
	}
	thread->prev = thread->next = NULL;
}

static void thread_add(struct thread* thread, struct thread* after) {
	thread->next = after->next;
	thread->prev = after;
	if (after->next != NULL) {
		after->next->prev = thread;
	}
	after->next = thread;
}

static void thread_fictive_init(struct thread* thread) {
	thread->prev = thread->next = NULL;
	thread->name = "FICTIVE";
}

void scheduler_init(void) {
	slab_init(&thread_allocator, sizeof(struct thread), _Alignof(struct thread));
	cs_init(&scheduler.cs);
	scheduler.alive = (struct thread*)slab_alloc(&thread_allocator);
	scheduler.alive_tail = scheduler.alive;
	scheduler.sleep = (struct thread*)slab_alloc(&thread_allocator);
	scheduler.dead = (struct thread*)slab_alloc(&thread_allocator);
	if (scheduler.dead == NULL) {
		halt("Not enough memory to setup scheduler.");
	}
	thread_fictive_init(scheduler.alive);
	thread_fictive_init(scheduler.sleep);
	thread_fictive_init(scheduler.dead);

	struct thread* main = (struct thread*) slab_alloc(&thread_allocator);
	if (main == NULL) {
		halt("Cannot allocate struct for idle (main) thread!");
	}
	cs_init(&main->cs);
	main->name = "main (idle)";
	main->prev = NULL;
	main->next = NULL;
	scheduler.current = main;

	is_mutlithread = true;
}

struct thread* thread_current(void) {
	return scheduler.current;
}

struct thread* thread_create(thread_func_t func, void* data, const char* name) {
	phys_t stack_phys = buddy_alloc(THREAD_STACK_ORDER);
	if (stack_phys == (phys_t)NULL)	 {
		return NULL;
	}

	struct thread* thread = (struct thread*) slab_alloc(&thread_allocator);
	if (thread == NULL) {
		buddy_free(stack_phys);
		return NULL;
	}

	cs_init(&thread->cs);
	thread->func = func;
	thread->data = data;
	thread->is_over = false;
	thread->name = name;

	thread->stack = va(stack_phys);
	thread->prev = NULL;
	thread->next = NULL;

	uint64_t* stack_top = (uint64_t*)((virt_t)thread->stack + THREAD_STACK_SIZE);

	void stack_push(uint64_t value) {
		--stack_top;
		*stack_top = value;
	}; // this semicolon just to shut up editor warning

	uint64_t rflags = read_rflags();
	rflags &= ~0b110011010101ll; // Clear status & direction flags
	rflags |=  1 << 9; // Set interruption flag

	stack_push((uint64_t) thread); // param for thread_run
	stack_push((uint64_t) thread_run_wrapper); // old RIP
	stack_push(rflags);
	stack_push(0); // RBP
	stack_push(0); // RBX
	stack_push(0); // R12
	stack_push(0); // R13
	stack_push(0); // R14
	stack_push(0); // R15
	thread->stack_pointer = stack_top;

	cs_enter(&scheduler.cs);
	thread_add(thread, scheduler.alive);
	if (scheduler.alive == scheduler.alive_tail) {
		scheduler.alive_tail = thread;
	}
	cs_leave(&scheduler.cs);

	return thread;
}

void thread_run(struct thread* thread) {
	thread_func_t func = thread->func;
	void* data = thread->data;

	// We should emulate exiting schedule() call
	if (scheduler.callback != NULL) {
		scheduler.callback();
	}
	interrupt_enable();
	log(LEVEL_V, "Starting thread %s.", thread->name);
	data = func(data);
	interrupt_disable();
	barrier();

	thread->is_over = true;
	thread->data = data;

	schedule(NULL, THREAD_NEW_STATE_DEAD);

	halt("Scheduler activated dead thread %s!", thread->name);
}

void* thread_join(struct thread* thread) {
	while (true) {
		cs_enter(&thread->cs);
		barrier();
		void* data = thread->data;
		bool is_over = thread->is_over;
		cs_leave(&thread->cs);

		if (is_over) {
			log(LEVEL_V, "Deleting dead thread %s.", thread->name);
			cs_enter(&scheduler.cs);
			thread_delete(thread);
			buddy_free(pa(thread->stack));
			slab_free(thread);
			cs_leave(&scheduler.cs);
			return data;
		} else {
			yield();
		}
	}
}

void schedule(schedule_callback_t callback, enum thread_new_state state) {
	// Interrupts should be off here!
	// For casual manual switching, use yield call!
	struct thread* current = scheduler.current;
	switch (state) {
		case THREAD_NEW_STATE_ALIVE:
			// Push old task to end...
			scheduler.alive_tail->next = current;
			current->prev = scheduler.alive_tail;
			scheduler.alive_tail = current;
			break;
		case THREAD_NEW_STATE_SLEEP:
			thread_add(current, scheduler.sleep);
			break;
		case THREAD_NEW_STATE_DEAD:
			thread_add(current, scheduler.dead);
	}
	// Get new task from beginning of alive threads...
	struct thread* target = scheduler.alive->next;
	if (target == NULL) {
		halt("There is no alive threads!");
	}
	thread_delete(target);

	scheduler.callback = callback;
	scheduler.current = target;

	log(LEVEL_V, "Initiate switching %s -> %s...", current->name, target->name);
	if (current == target) {
		log(LEVEL_WARN, "Oh, there are the same! Not switching...");
	} else {
		barrier();
		thread_switch(&current->stack_pointer, target->stack_pointer);
		barrier();
		log(LEVEL_V, "Switched back to %s.", scheduler.current->name);
	}

	if (scheduler.callback != NULL) {
		scheduler.callback();
	}
}

void yield(void) {
	interrupt_disable();
	schedule(NULL, THREAD_NEW_STATE_ALIVE);
	interrupt_enable();
}
