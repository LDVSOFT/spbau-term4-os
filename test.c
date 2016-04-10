#include "test.h"
#include "threads.h"
#include "log.h"
#include "print.h"

#include <stddef.h>

struct thread_test_data {
	int level;
	uint64_t id;
};

const int MAX_LEVEL = 10;
const int BUFFER = 128;

static void* thread_test(struct thread_test_data* data) {
	log(LEVEL_V, "Enter, data=%p.", data);
	if (data->level < MAX_LEVEL) {
		struct thread_test_data left_data;
		left_data.level = data->level + 1;
		left_data.id = data->id * 2;
		char left_name[BUFFER];
		snprintf(left_name, BUFFER, "test %d", left_data.id);
		struct thread* left = thread_create((thread_func_t)thread_test, &left_data, left_name);
		log(LEVEL_V, "Created left = %p.", left);

		struct thread_test_data right_data;
		right_data.level = data->level + 1;
		right_data.id = data->id * 2 + 1;
		char right_name[BUFFER];
		snprintf(right_name, BUFFER, "test %d", right_data.id);
		struct thread* right = thread_create((thread_func_t)thread_test, &right_data, right_name);
		log(LEVEL_V, "Created right = %p.", right);

		if ((uint64_t)thread_join(left) != left_data.id) {
			halt("Left return wrong id.");
		}
		if ((uint64_t)thread_join(right) != right_data.id) {
			halt("Right return wrong id.");
		}
	}
	log(LEVEL_V, "Exit");
	return (void*)data->id;
}

void test_threads(void) {
	log(LEVEL_INFO, "Starting threads test...");

	struct thread_test_data root;
	root.level = 0;
	root.id = 1;
	thread_test(&root);

	log(LEVEL_INFO, "Threads test completed.");
}

struct test_cv_data {
	struct mutex cs;
	struct condition_variable cv;
	bool is_set;
};

static void* test_cv_waiter(void* p) {
	struct test_cv_data* data = (struct test_cv_data*)p;
	mutex_lock(&data->cs);
	if (data->is_set) {
		halt("Too slow...");
	}
	while (!data->is_set) {
		cv_wait(&data->cv);
		log(LEVEL_V, "I woke up!");
		barrier();
	}
	log(LEVEL_V, "Finally!");
	mutex_unlock(&data->cs);
	return NULL;
}

static void* test_cv_setter(void* p) {
	struct test_cv_data* data = (struct test_cv_data*)p;
	mutex_lock(&data->cs);
	int a = 0;
	for (int i = 0; i != 2000; ++i) {
		for (int j = 0; j != 2000; ++j) {
			for (int k = 0; k != 2000; ++k) {
				a += i * 2 + j + k * 34;
			}
		}
		if (i % 100 == 0) {
			yield();
		}
	}
	log(LEVEL_V, "Setting %d.", a);
	data->is_set = a != 0;
	cv_notify(&data->cv);
	log(LEVEL_V, "Notified?");
	mutex_unlock(&data->cs);
	return NULL;
}

void test_condition_variable() {
	log(LEVEL_INFO, "Starting condition variable test...");
	struct test_cv_data data;
	mutex_init(&data.cs);
	cv_init(&data.cv, &data.cs);
	data.is_set = false;
	barrier();

	log(LEVEL_V, "Starting waiter...");
	struct thread* waiter = thread_create(test_cv_waiter, &data, "waiter");
	log(LEVEL_V, "Switching...");
	yield();
	log(LEVEL_V, "Starting setter...");
	struct thread* setter = thread_create(test_cv_setter, &data, "setter");
	log(LEVEL_V, "Waiting...");
	thread_join(waiter);
	thread_join(setter);

	cv_finit(&data.cv);
	mutex_finit(&data.cs);
	log(LEVEL_INFO, "Condition variable test completed.");
}
