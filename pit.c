#include "pit.h"
#include "print.h"
#include "kernel_config.h"
#include "pic.h"

void pit_init(const struct idt_ptr *idt_ptr) {
	out8(PORT_PIT_CONTROL, PIT_COMMAND_SET_RATE_GENERATOR);
	out8(PORT_PIT_DATA, (PIT_DIVISOR >> 0) & 0xFF);
	out8(PORT_PIT_DATA, (PIT_DIVISOR >> 2) & 0xFF);

	interrupt_set(idt_ptr, INTERRUPT_PIT, (uint64_t) &pit_handler_wrapper);
}

void pit_handler() {
	static int counter = 0;
	static int number = 0;
	++counter;
	if (counter >= (PIT_FREQUENCY * PIT_INTERVAL) / PIT_DIVISOR) {
		++number;
		printf("Hello, user! [Message #%d]\n", number);
		counter = 0;
	}
	pic_eoi(0);

}
