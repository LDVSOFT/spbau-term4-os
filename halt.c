#include "halt.h"
#include "print.h"
#include "interrupt.h"

void halt(const char *s, ...) {
	va_list args;
	va_start(args, s);
	printf("SYSTEM HALTED\n");
	vprintf(s, args);
	va_end(args);

	interrupt_disable();
	while (1)
		hlt();
}
