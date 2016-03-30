#include "test.h"
#include "threads.h"
#include "log.h"

struct thread_test_data {
	int level;
	uint64_t id;
};

const int MAX_LEVEL = 10;

static void* thread_test(struct thread_test_data* data) {
	log(LEVEL_V, "%d: Enter, data=%p.", data->id, data);
	if (data->level < MAX_LEVEL) {
		struct thread_test_data left_data;
		left_data.level = data->level + 1;
		left_data.id = data->id * 2;
		struct thread* left = thread_create((thread_func_t)thread_test, &left_data, "test");
		log(LEVEL_V, "%d: Created left = %p.", data->id, left);
		struct thread_test_data right_data;
		right_data.level = data->level + 1;
		right_data.id = data->id * 2 + 1;
		struct thread* right = thread_create((thread_func_t)thread_test, &right_data, "test");
		log(LEVEL_V, "%d: Created right = %p.", data->id, right);
		if ((uint64_t)thread_join(left) != left_data.id) {
			halt("%d: Left return wrong id.");
		}
		if ((uint64_t)thread_join(right) != right_data.id) {
			halt("%d: Right return wrong id.");
		}
	}
	log(LEVEL_V, "%d: Exit", data->id);
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
