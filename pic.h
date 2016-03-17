#pragma once

#include "ioport.h"
#include <stdbool.h>

#define PORT_PIC_MASTER_COMMAND 0x20
#define PORT_PIC_MASTER_DATA    0x21
#define PORT_PIC_SLAVE_COMMAND  0xA0
#define PORT_PIC_SLAVE_DATA     0xA1

#define PIC_COMMAND_INIT 0b00010001
#define PIC_COMMAND_EOI  0b00100000
#define PIC_SLAVE_PORT   2
#define PIC_MODE         1

void pic_init();
void pic_eoi(bool is_slave);
