#include "kernel_config.h"
#include "interrupt.h"
#include "serial.h"
#include "pit.h"
#include "pic.h"
#include "log.h"
#include "memory.h"
#include "buddy.h"
#include "paging.h"
#include "slab-allocator.h"
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

	interrupt_enable();

	while (true) {
		hlt();
	}
}
