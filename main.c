#include "serial.h"

void main(void) {
	serial_init();
	while (1) {
		serial_puts("Hello!\n");

		int a = 0;
		for (int i = 0; i != 10000; ++i) {
			for (int j = 0; j != 10000; ++j) {
				a += i * 6 + j * 34;
			}
		}
	}
}
