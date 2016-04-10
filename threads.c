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
	struct list_node alive;
	struct list_node sleep;
	struct list_node dead;
} scheduler;
static bool is_multithreaded = false;

static void thread_fictive_init(struct thread* thread) {
	list_init(&thread->scheduler_link);
	list_init(&thread->store_link);
	thread->name = "FICTIVE";
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

void cv_init(struct condition_variable* variable, struct mutex* mutex) {
	variable->mutex = mutex;
	list_init(&variable->threads_head);
}

void cv_finit(struct condition_variable* variable) {
}

void cv_wait(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	struct thread* current = thread_current();
	// idle thread does not sleeps!
	bool can_sleep = current != &scheduler.idle;
	bool should_release = variable != &variable->mutex->is_locked;
	if (can_sleep) {
		list_add(&current->store_link, &variable->threads_head);
	}
	// If that variable is not for locking, release...
	if (should_release) {
		mutex_unlock(variable->mutex);
	}
	if (can_sleep) {
		// Alive -> sleep
		schedule(THREAD_NEW_STATE_SLEEP);
	} else {
		schedule(THREAD_NEW_STATE_ALIVE);
	}
	// ..and take back here.
	if (should_release) {
		mutex_lock(variable->mutex);
	}
	hard_unlock(rflags);
}

static void __cv_notify(struct condition_variable* variable, struct thread* wake_up) {
	if (wake_up == NULL) {
		log(LEVEL_VVV, "Noone to notify! %p.", variable);
		return;
	}
	log(LEVEL_VVV, "Notified %s.", wake_up->name);
	list_delete(&wake_up->store_link);
	// Sleep -> alive
	list_delete(&wake_up->scheduler_link);
	list_add(&wake_up->scheduler_link, &scheduler.alive);
}

void cv_notify(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	if (!list_empty(&variable->threads_head)) {
		struct thread* wake_up = LIST_ENTRY(list_first(&variable->threads_head), struct thread, store_link);
		__cv_notify(variable, wake_up);
	}
	hard_unlock(rflags);
}

void cv_notify_all(struct condition_variable* variable) {
	uint64_t rflags = hard_lock();
	while (!list_empty(&variable->threads_head)) {
		struct thread* wake_up = LIST_ENTRY(list_first(&variable->threads_head), struct thread, store_link);
		__cv_notify(variable, wake_up);
	}
	hard_unlock(rflags);
}

void mutex_init(struct mutex* mutex) {
	mutex->is_occupied = false;
	cv_init(&mutex->is_locked, mutex);
}

void mutex_finit(struct mutex* mutex) {
	cv_finit(&mutex->is_locked);
}

void mutex_lock(struct mutex* mutex) {
	uint64_t rflags = hard_lock();
	if (is_multithreaded) {
		while (mutex->is_occupied) {
			cv_wait(&mutex->is_locked);
		}
		mutex->is_occupied = true;
	}
	hard_unlock(rflags);
}

void mutex_unlock(struct mutex* mutex) {
	uint64_t rflags = hard_lock();
	if (is_multithreaded) {
		cv_notify(&mutex->is_locked);
		mutex->is_occupied = false;
	}
	hard_unlock(rflags);
}

static struct slab_allocator thread_allocator;

extern void* init_stack;

void scheduler_init(void) {
	slab_init_for(&thread_allocator, struct thread);
	list_init(&scheduler.alive);
	list_init(&scheduler.sleep);
	list_init(&scheduler.dead);

	struct thread* main = &scheduler.idle;
	mutex_init(&main->lock);
	cv_init(&main->is_dead, &main->lock);
	main->name = "main (idle)";
	list_init(&main->scheduler_link);
	list_init(&main->store_link);
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
	mutex_init(&thread->lock);
	cv_init(&thread->is_dead, &thread->lock);
	thread->func = func;
	thread->data = data;
	thread->is_over = false;
	thread->name = name;

	thread->stack = va(stack_phys);
	list_init(&thread->scheduler_link);
	list_init(&thread->store_link);

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
	list_add(&thread->scheduler_link, &scheduler.alive);
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

	mutex_lock(&thread->lock);
	thread->is_over = true;
	thread->data = data;
	cv_notify(&thread->is_dead);
	mutex_unlock(&thread->lock);

	schedule(THREAD_NEW_STATE_DEAD);
	hard_unlock(rflags);
	halt("Scheduler activated dead thread %s!", thread->name);
}

void* thread_join(struct thread* thread) {
	mutex_lock(&thread->lock);
	while (!thread->is_over) {
		cv_wait(&thread->is_dead);
	}
	
	void* data = (void*) thread->data;
	mutex_unlock(&thread->lock);
	log(LEVEL_VV, "Deleting dead thread %s.", thread->name);

	uint64_t rflags = hard_lock();
	list_delete(&thread->scheduler_link);
	cv_finit(&thread->is_dead);
	mutex_finit(&thread->lock);
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
			list_add_tail(&current->scheduler_link, &scheduler.alive);
			break;
		case THREAD_NEW_STATE_SLEEP:
			list_add(&current->scheduler_link, &scheduler.sleep);
			break;
		case THREAD_NEW_STATE_DEAD:
			list_add(&current->scheduler_link, &scheduler.dead);
			break;
	}
	// Get new task from beginning of alive threads...
	if (list_empty(&scheduler.alive)) {
		halt("There is no alive threads!");
	}
	struct thread* target = LIST_ENTRY(list_first(&scheduler.alive), struct thread, scheduler_link);
	list_delete(&target->scheduler_link);

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
