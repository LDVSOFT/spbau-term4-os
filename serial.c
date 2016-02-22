#include "serial.h"
#include "kernel_config.h"
#include "memory.h"

void serial_init() {
	out8(PORT_SERIAL_BASE + 3, SERIAL_FLAG_FRAME | SERIAL_FLAG_DLAB);
	out8(PORT_SERIAL_BASE + 0, get_bits(SERIAL_DIVISOR, 0, 8));
	out8(PORT_SERIAL_BASE + 1, get_bits(SERIAL_DIVISOR, 8, 8));

	out8(PORT_SERIAL_BASE + 3, SERIAL_FLAG_FRAME);
	out8(PORT_SERIAL_BASE + 1, 0);
}

void serial_putch(char c) {
	while ((in8(PORT_SERIAL_BASE + 5) & SERIAL_FLAG_WRITE_COMPLETE) == 0) {
		;
	}
	out8(PORT_SERIAL_BASE + 0, c);
}

void serial_puts(const char *s) {
	for (; *s != 0; ++s) {
		serial_putch(*s);
	}
}
