#include "pit.h"
#include "kernel_config.h"
#include "pic.h"
#include "memory.h"
#include "threads.h"

static void pit_eoi(void) {
	pic_eoi(false);
}

void pit_handler(struct interrupt_info* info) {
	static int counter = 0;
	static int number = 0;
	++counter;
	if (counter >= PIT_TICKS) {
		++number;
		counter = 0;
		schedule(pit_eoi, THREAD_NEW_STATE_ALIVE);
	} else {
		pic_eoi(false);
	}
}

void pit_init(void) {
	out8(PORT_PIT_CONTROL, PIT_COMMAND_SET_RATE_GENERATOR);
	out8(PORT_PIT_DATA, get_bits(PIT_DIVISOR, 0, 8));
	out8(PORT_PIT_DATA, get_bits(PIT_DIVISOR, 8, 8));

	interrupt_set(INTERRUPT_PIT, pit_handler);
}
