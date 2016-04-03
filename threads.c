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

static uint64_t hard_lock() {
	uint64_t rflags = read_rflags();
	interrupt_disable();
	return rflags;
}

static void hard_unlock(uint64_t rflags) {
	write_rflags(rflags);
}

void cs_init(struct critical_section* section) {
	section->is_occupied = false;
}

void cs_finit(struct critical_section* section) {
}

void cs_enter(struct critical_section* section) {
	uint64_t rflags = hard_lock();
	barrier();
	while (section->is_occupied) {
		schedule(NULL, THREAD_NEW_STATE_ALIVE);
		barrier();
	}
	section->is_occupied = true;
	barrier();
	hard_unlock(rflags);
}

void cs_leave(struct critical_section* section) {
	uint64_t rflags = hard_lock();
	section->is_occupied = false;
	barrier();
	hard_unlock(rflags);
}

static struct slab_allocator thread_allocator;
static struct slab_allocator thread_cs_allocator;

// There always must be at least one alive thread (idle for example)
static struct {
	schedule_callback_t callback;
	struct thread* current;
	struct thread alive;
	struct thread* alive_tail;
	struct thread sleep;
	struct thread dead;
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
	slab_init(&thread_cs_allocator, sizeof(struct critical_section), _Alignof(struct critical_section));
	scheduler.alive_tail = &scheduler.alive;
	thread_fictive_init(&scheduler.alive);
	thread_fictive_init(&scheduler.sleep);
	thread_fictive_init(&scheduler.dead);

	struct thread* main = (struct thread*) slab_alloc(&thread_allocator);
	struct critical_section* main_cs = (struct critical_section*) slab_alloc(&thread_cs_allocator);
	if (main == NULL || main_cs == NULL) {
		halt("Cannot allocate struct for idle (main) thread!");
	}
	main->cs = main_cs;
	cs_init(main->cs);
	main->name = "main (idle)";
	main->prev = NULL;
	main->next = NULL;
	scheduler.current = main;
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
	thread->cs = (struct critical_section*) slab_alloc(&thread_cs_allocator);
	if (thread->cs == NULL) {
		slab_free(thread);
		buddy_free(stack_phys);
		return NULL;
	}

	cs_init(thread->cs);
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

	uint64_t new_rflags = read_rflags();
	new_rflags &= ~0b110011010101ll; // Clear status & direction flags; iterrupts are off already

	stack_push((uint64_t) thread); // param for thread_run
	stack_push((uint64_t) thread_run_wrapper); // old RIP
	stack_push(new_rflags);
	stack_push(0); // RBP
	stack_push(0); // RBX
	stack_push(0); // R12
	stack_push(0); // R13
	stack_push(0); // R14
	stack_push(0); // R15
	thread->stack_pointer = stack_top;

	// Scheduler must be hard-locked
	uint64_t rflags = hard_lock();
	thread_add(thread, &scheduler.alive);
	if (&scheduler.alive == scheduler.alive_tail) {
		scheduler.alive_tail = thread;
	}
	hard_unlock(rflags);

	return thread;
}

void thread_run(struct thread* thread) {
	thread_func_t func = thread->func;
	void* data = thread->data;

	// We should emulate exiting schedule() call
	if (scheduler.callback != NULL) {
		scheduler.callback();
	}
	// Thread is created with disabled interrupts
	interrupt_enable();
	log(LEVEL_V, "Starting thread %s.", thread->name);

	data = func(data);
	barrier();

	cs_enter(thread->cs);
	thread->is_over = true;
	thread->data = data;
	barrier();
	cs_leave(thread->cs);

	uint64_t rflags = hard_lock();
	schedule(NULL, THREAD_NEW_STATE_DEAD);
	hard_unlock(rflags);
	halt("Scheduler activated dead thread %s!", thread->name);
}

void* thread_join(struct thread* thread) {
	while (true) {
		cs_enter(thread->cs);
		barrier();
		void* data = thread->data;
		bool is_over = thread->is_over;
		cs_leave(thread->cs);

		if (is_over) {
			log(LEVEL_V, "Deleting dead thread %s.", thread->name);
			uint64_t rflags = hard_lock();
			thread_delete(thread);
			buddy_free(pa(thread->stack));
			slab_free(thread);
			hard_unlock(rflags);
			return data;
		} else {
			yield();
		}
	}
}

void schedule(schedule_callback_t callback, enum thread_new_state state) {
	// Interrupts should be off here (for example, via hard_lock())!
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
			thread_add(current, &scheduler.sleep);
			break;
		case THREAD_NEW_STATE_DEAD:
			thread_add(current, &scheduler.dead);
	}
	// Get new task from beginning of alive threads...
	struct thread* target = scheduler.alive.next;
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
	uint64_t rflags = hard_lock();
	schedule(NULL, THREAD_NEW_STATE_ALIVE);
	hard_unlock(rflags);
}
