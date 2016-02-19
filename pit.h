#ifndef __PIT_H__
#define __PIT_H__

#include "interrupt.h"

#define INTERRUPT_PIT (INTERRUPT_PIC_MASTER + 0)

#define PORT_PIT_DATA    0x40
#define PORT_PIT_CONTROL 0x43

#define PIT_FREQUENCY 1193180
#define PIT_COMMAND_SET_RATE_GENERATOR 0b00110100

void pit_init(const struct idt_ptr *idt_ptr);
void pit_handler_wrapper();
void pit_handler();

#endif
