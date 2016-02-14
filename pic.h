#ifndef __PIC_H__
#define __PIC_H__

#include "ioport.h"
#include "interrupt.h"

#define PORT_PIC_MASTER_COMMAND 0x20
#define PORT_PIC_MASTER_DATA    0x21
#define PORT_PIC_SLAVE_COMMAND  0xA0
#define PORT_PIC_SLAVE_DATA     0xA1

#define PIC_COMMAND_INIT 0b00010001
#define PIC_COMMAND_EOI  0b00100000
#define PIC_SLAVE_PORT   2
#define PIC_MODE         1

static void pic_init() {
	// Master
	out8(PORT_PIC_MASTER_COMMAND, PIC_COMMAND_INIT);
	out8(PORT_PIC_MASTER_DATA,    INTERRUPT_PIC_MASTER);
	out8(PORT_PIC_MASTER_DATA,    1 << PIC_SLAVE_PORT);
	out8(PORT_PIC_MASTER_DATA,    PIC_MODE);

	// Slave
	out8(PORT_PIC_SLAVE_COMMAND, PIC_COMMAND_INIT);
	out8(PORT_PIC_SLAVE_DATA,    INTERRUPT_PIC_SLAVE);
	out8(PORT_PIC_SLAVE_DATA,    PIC_SLAVE_PORT /*sic!*/);
	out8(PORT_PIC_SLAVE_DATA,    PIC_MODE);
}

static void pic_eoi(int is_slave) {
	// Slave
	if (is_slave) {
		out8(PORT_PIC_SLAVE_COMMAND, PIC_COMMAND_EOI);
	}

	// Master
	out8(PORT_PIC_MASTER_COMMAND, PIC_COMMAND_EOI);
}

#endif /*__PIC_H__*/
