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

// There always must be at least one alive thread (idle for example)
static struct {
	struct thread idle;
	struct thread* current;
	struct thread alive;
	struct thread* alive_tail;
	struct thread sleep;
	struct thread dead;
} scheduler;
static bool is_multithreaded = false;

static void thread_fictive_init(struct thread* thread) {
	thread->prev = thread->next = NULL;
	thread->alt_prev = thread->alt_next = NULL;
	thread->name = "FICTIVE";
}

static void thread_add(struct thread* thread, struct thread* after) {
	thread->next = after->next;
	thread->prev = after;
	if (after->next != NULL) {
		after->next->prev = thread;
	}
	after->next = thread;
}

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

static void thread_add_alt(struct thread* thread, struct thread* after) {
	thread->alt_next = after->alt_next;
	thread->alt_prev = after;
	if (after->alt_next != NULL) {
		after->alt_next->alt_prev = thread;
	}
	after->alt_next = thread;
}

static void thread_delete_alt(struct thread* thread) {
	if (thread->alt_prev != NULL) {
		thread->alt_prev->alt_next = thread->alt_next;
	}
	if (thread->alt_next != NULL) {
		thread->alt_next->alt_prev = thread->alt_prev;
	}
	thread->alt_prev = thread->alt_next = NULL;
}

// Locks

static uint64_t hard_lock() {
	uint64_t rflags = read_rflags();
	interrupt_disable();
	barrier();
	return rflags;
}

static void hard_unlock(uint64_t rflags) {
	barrier();
	write_rflags(rflags);
}

void cv_init(struct condition_variable* variable, struct critical_section* section) {
	variable->section = section;
	thread_fictive_init(&variable->head);
}

void cv_finit(struct condition_variable* variable) {
}

void cv_wait(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	struct thread* current = thread_current();
	// idle thread does not sleeps!
	bool can_sleep = current != &scheduler.idle;
	bool should_release = variable != &variable->section->is_locked;
	if (can_sleep) {
		thread_add_alt(current, &variable->head);
	}
	// If that variable is not for locking, release...
	if (should_release) {
		cs_leave(variable->section);
	}
	if (can_sleep) {
		// Alive -> sleep
		schedule(THREAD_NEW_STATE_SLEEP);
	} else {
		schedule(THREAD_NEW_STATE_ALIVE);
	}
	// ..and take back here.
	if (should_release) {
		cs_enter(variable->section);
	}
	hard_unlock(rflags);
}

static void __cv_notify(struct condition_variable* variable, struct thread* wake_up) {
	if (wake_up == NULL) {
		log(LEVEL_VVV, "Noone to notify! %p.", variable);
		return;
	}
	log(LEVEL_VVV, "Notified %s.", wake_up->name);
	thread_delete_alt(wake_up);
	// Sleep -> alive
	thread_delete(wake_up);
	thread_add(wake_up, &scheduler.alive);
}

void cv_notify(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	struct thread* wake_up = variable->head.alt_next;
	__cv_notify(variable, wake_up);
	hard_unlock(rflags);
}

void cv_notify_all(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	struct thread* it = variable->head.alt_next;
	while (it != NULL) {
		__cv_notify(variable, it);
		it = variable->head.alt_next;
	}
	hard_unlock(rflags);
}

void cs_init(struct critical_section* section) {
	section->is_occupied = false;
	cv_init(&section->is_locked, section);
}

void cs_finit(struct critical_section* section) {
	cv_finit(&section->is_locked);
}

void cs_enter(struct critical_section* section) {
	uint64_t rflags = hard_lock();
	if (is_multithreaded) {
		while (section->is_occupied) {
			cv_wait(&section->is_locked);
		}
		section->is_occupied = true;
	}
	hard_unlock(rflags);
}

void cs_leave(struct critical_section* section) {
	uint64_t rflags = hard_lock();
	if (is_multithreaded) {
		cv_notify(&section->is_locked);
		section->is_occupied = false;
	}
	hard_unlock(rflags);
}

static struct slab_allocator thread_allocator;
static struct slab_allocator thread_cs_allocator;
static struct slab_allocator thread_cv_allocator;

extern void* init_stack;

void scheduler_init(void) {
	slab_init_for(&thread_allocator, struct thread);
	slab_init_for(&thread_cs_allocator, struct critical_section);
	slab_init_for(&thread_cv_allocator, struct condition_variable);
	scheduler.alive_tail = &scheduler.alive;
	thread_fictive_init(&scheduler.alive);
	thread_fictive_init(&scheduler.sleep);
	thread_fictive_init(&scheduler.dead);

	struct thread* main = &scheduler.idle;
	struct critical_section* main_cs = (struct critical_section*) slab_alloc(&thread_cs_allocator);
	struct condition_variable* main_cv = (struct condition_variable*) slab_alloc(&thread_cv_allocator);
	if (main_cs == NULL || main_cv == NULL) {
		halt("Cannot allocate struct for idle (main) thread!");
	}
	main->cs = main_cs;
	main->is_dead = main_cv;
	cs_init(main->cs);
	cv_init(main->is_dead, main_cs);
	main->name = "main (idle)";
	main->prev = NULL;
	main->next = NULL;
	main->stack = init_stack;
	scheduler.current = main;

	is_multithreaded = true;
}

struct thread* thread_current(void) {
	return scheduler.current;
}

struct thread* thread_create(thread_func_t func, void* data, const char* name) {
	phys_t stack_phys = buddy_alloc(THREAD_STACK_ORDER);
	if (stack_phys == (phys_t)NULL)	{
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
	thread->is_dead = (struct condition_variable*) slab_alloc(&thread_cv_allocator);
	if (thread->is_dead == NULL) {
		slab_free((struct critical_section*) thread->cs);
		slab_free(thread);
		buddy_free(stack_phys);
		return NULL;
	}

	cs_init(thread->cs);
	cv_init(thread->is_dead, thread->cs);
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
	new_rflags &= ~0b111011010101ll; // Clear status, direction & interrupt

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

	// Thread is created with disabled interrupts
	interrupt_enable();
	log(LEVEL_VV, "Starting thread %s.", thread->name);

	data = func((void*) data);

	uint64_t rflags = hard_lock();

	cs_enter(thread->cs);
	thread->is_over = true;
	thread->data = data;
	cv_notify(thread->is_dead);
	cs_leave(thread->cs);

	schedule(THREAD_NEW_STATE_DEAD);
	hard_unlock(rflags);
	halt("Scheduler activated dead thread %s!", thread->name);
}

void* thread_join(struct thread* thread) {
	cs_enter(thread->cs);
	while (!thread->is_over) {
		cv_wait(thread->is_dead);
	}
	
	void* data = (void*) thread->data;
	cs_leave(thread->cs);
	log(LEVEL_VV, "Deleting dead thread %s.", thread->name);

	uint64_t rflags = hard_lock();
	thread_delete(thread);
	buddy_free(pa(thread->stack));
	slab_free(thread);
	hard_unlock(rflags);
	
	return data;
}

void schedule(enum thread_new_state state) {
	uint64_t rflags = hard_lock();
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

	scheduler.current = target;

	log(LEVEL_VV, "Initiate switching %s -> %s...", current->name, target->name);
	if (current == target) {
		log(LEVEL_WARN, "Oh, there are the same! Not switching...");
	} else {
		thread_switch(&current->stack_pointer, target->stack_pointer);
		log(LEVEL_VV, "Switched to %s.", scheduler.current->name);
	}
	hard_unlock(rflags);
}

void yield(void) {
	schedule(THREAD_NEW_STATE_ALIVE);
}
