#include "kernel_config.h"
#include "interrupt.h"
#include "serial.h"
#include "pit.h"
#include "pic.h"
#include "log.h"
#include "print.h"
#include "memory.h"
#include "buddy.h"
#include "paging.h"
#include "slab-allocator.h"
#include "threads.h"

#include <stdbool.h>

void init_memory(void) {
	log(LEVEL_VVV, "Original MMAP:");
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	print_mmap(va(info->mmap_addr), info->mmap_length);

	buddy_init();
	paging_build();
	buddy_init_high();
	slab_allocators_init();
}

struct thread_test_data {
	int level;
	uint64_t id;
};

const int MAX_LEVEL = 6;

void* thread_test(struct thread_test_data* data) {
	log(LEVEL_LOG, "%d: Enter, data=%p.", data->id, data);
	if (data->level < MAX_LEVEL) {
		struct thread_test_data left_data;
		left_data.level = data->level + 1;
		left_data.id = data->id * 2;
		struct thread* left = thread_create((thread_func_t)thread_test, &left_data, "test");
		log(LEVEL_LOG, "%d: Created left = %p.", data->id, left);
		struct thread_test_data right_data;
		right_data.level = data->level + 1;
		right_data.id = data->id * 2 + 1;
		struct thread* right = thread_create((thread_func_t)thread_test, &right_data, "test");
		log(LEVEL_LOG, "%d: Created right = %p.", data->id, right);
		if ((uint64_t)thread_join(left) != left_data.id) {
			halt("%d: Left return wrong id.");
		}
		if ((uint64_t)thread_join(right) != right_data.id) {
			halt("%d: Right return wrong id.");
		}
	}
	log(LEVEL_LOG, "%d: Exit", data->id);
	return (void*)data->id;
}

void main(void) {
	log_set_color_enabled(LOG_COLOR);
	log_set_level(LOG_LEVEL);

	serial_init();
	pic_init();
	log(LEVEL_INFO, "Serial & pic ready.");

	init_memory();
	log(LEVEL_INFO, "Memory is ready.");

	interrupt_init();
	pit_init();
	log(LEVEL_INFO, "IDT & pit are ready.");

	scheduler_init();
	interrupt_enable();
	log(LEVEL_INFO, "Scheduler is ready; multithreading is on!");

	struct thread_test_data root;
	root.level = 0;
	root.id = 1;
	thread_test(&root);

	while (true) {
		int a = 0;
		for (int i = 0; i != 1000; ++i) {
			for (int j = 0; j != 1000; ++j) {
				for (int k = 0; k != 1000; ++k) {
					a += i + 2 * j + 3 * k;
				}
			}
		}
		printf("Hello from main %d\n!", a);
		hlt();
	}
}
