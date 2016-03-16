#include "kernel_config.h"
#include "interrupt.h"
#include "serial.h"
#include "pit.h"
#include "pic.h"
#include "log.h"
#include "memory.h"
#include "buddy.h"
#include "paging.h"

idt_t idt;
struct idt_ptr idt_ptr;

void init_memory() {
	log(LEVEL_LOG, "Original MMAP:");
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	print_mmap(va(info->mmap_addr), info->mmap_length);

	buddy_init();
	paging_build();
	buddy_init_high();
}

void main(void) {
	log_set_color_enabled(LOG_COLOR);
	log_set_level(LOG_LEVEL);

	serial_init();
	pic_init();
	log(LEVEL_INFO, "Serial & pic ready.");

	init_memory();
	log(LEVEL_INFO, "Memory ready.");

	interrupt_init_ptr(&idt_ptr, idt);
	pit_init(&idt_ptr);
	interrupt_set_idt(&idt_ptr);
	log(LEVEL_INFO, "IDT & pit are ready.");

	interrupt_enable();

	while (1) {
		hlt();
	}
}
