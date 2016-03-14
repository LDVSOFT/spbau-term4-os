#include "interrupt.h"
#include "serial.h"
#include "pit.h"
#include "pic.h"

idt_t idt;
struct idt_ptr idt_ptr;

void main(void) {
	serial_init();
	pic_init();

	serial_puts("Serial & pic ready.\n");

	interrupt_init_ptr(&idt_ptr, idt);
	pit_init(&idt_ptr);
	interrupt_set_idt(&idt_ptr);

	serial_puts("IDT & pit are ready.\n");

	interrupt_enable();

	while (1) {
		hlt();
	}
}
