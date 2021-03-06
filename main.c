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
#include "cmdline.h"
#include "test.h"
#include "fs.h"
#include "string.h"
#include "initramfs.h"
#include "multiboot.h"

#include <stdbool.h>

void init_memory(void) {
	log(LEVEL_VVV, "Original MMAP:");
	struct mboot_info* info = mboot_info_get();
	print_mmap(va(info->mmap_addr), info->mmap_length);

	buddy_init();
	paging_build();
	buddy_init_high();
	slab_allocators_init();
}

void main(void) {
	log_set_level(LEVEL_LOG);
	log_set_color_enabled(false);

	serial_init();
	log(LEVEL_INFO, "Serial ready.");

	read_cmdline();

	log(LEVEL_INFO, "Preparing memory...");
	init_memory();
	log(LEVEL_INFO, "Memory is ready.");

	log(LEVEL_INFO, "Preparing PIC...");
	pic_init();
	log(LEVEL_INFO, "PIC is ready.");

	log(LEVEL_INFO, "Preparing interrupts...");
	interrupt_init();
	log(LEVEL_INFO, "Interrputs are ready.");

	log(LEVEL_INFO, "Preparing PIT...");
	pit_init();
	log(LEVEL_INFO, "PIT is ready.");

	log(LEVEL_INFO, "Preparing scheduler...");
	scheduler_init();
	log(LEVEL_INFO, "Scheduler is ready, multithreading is on.");
	interrupt_enable();

	log(LEVEL_INFO, "Preparing file system...");
	fs_init();
	log(LEVEL_INFO, "File system is ready.");

	log(LEVEL_INFO, "Loading initramfs...");
	initramfs_load();
	log(LEVEL_INFO, "Loading done. Starting ls()...");
	ls();
	log(LEVEL_INFO, "ls() done.");

	#ifdef CONFIG_TESTS
	printf("Starting tests!\n");
	test_threads();
	test_condition_variable();
	#endif

	while (true) {
		int a = 0;
		for (int i = 0; i != 1000; ++i) {
			for (int j = 0; j != 1000; ++j) {
				for (int k = 0; k != 1000; ++k) {
					a += i + 2 * j + 3 * k;
				}
			}
		}
		printf("Hello from main %d!\n", a);
		hlt();
	}
}
