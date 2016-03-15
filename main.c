#include "interrupt.h"
#include "serial.h"
#include "pit.h"
#include "pic.h"
#include "print.h"
#include "memory.h"
#include "bootstrap-alloc.h"
#include "buddy.h"

idt_t idt;
struct idt_ptr idt_ptr;

void init_memory() {
	printf("Original MMAP:\n");
	struct mboot_info* info = (struct mboot_info*)va(mboot_info);
	print_mmap(va(info->mmap_addr), info->mmap_length);

	buddy_init();
	printf("Bootstrap MMAP (AFTER buddy init):\n");
	print_mmap(bootstrap_mmap, bootstrap_mmap_length);
}

void main(void) {
	serial_init();
	pic_init();

	serial_puts("Serial & pic ready.\n");
	init_memory();

	interrupt_init_ptr(&idt_ptr, idt);
	pit_init(&idt_ptr);
	interrupt_set_idt(&idt_ptr);

	serial_puts("IDT & pit are ready.\n");

	interrupt_enable();

	while (1) {
		hlt();
	}
}
