#include "serial.h"
#include "kernel_config.h"

void serial_init() {
	out8(PORT_SERIAL_BASE + 3, SERIAL_FLAG_FRAME | SERIAL_FLAG_DLAB);
	out8(PORT_SERIAL_BASE + 0, (SERIAL_DIVISOR >> 0) & 0xFF);
	out8(PORT_SERIAL_BASE + 1, (SERIAL_DIVISOR >> 2) & 0xFF);

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
