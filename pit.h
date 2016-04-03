#pragma once

#include "interrupt.h"
#include <stdint.h>

#define PORT_PIT_DATA    0x40
#define PORT_PIT_CONTROL 0x43

#define PIT_FREQUENCY 1193180
#define PIT_COMMAND_SET_RATE_GENERATOR 0b00110100

void pit_init(void);
