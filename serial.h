#pragma once

#include "ioport.h"

#define PORT_SERIAL_BASE 0x3F8

#define SERIAL_FLAG_DLAB (1 << 7)
#define SERIAL_FLAG_FRAME 0b11

#define SERIAL_FLAG_WRITE_COMPLETE (1 << 5)

void serial_init();
void serial_putch(char c);
void serial_puts(const char *s);
