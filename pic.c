#include "pic.h"
#include "interrupt.h"

void pic_init(void) {
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

void pic_eoi(bool is_slave) {
	// Slave
	if (is_slave) {
		out8(PORT_PIC_SLAVE_COMMAND, PIC_COMMAND_EOI);
	}

	// Master
	out8(PORT_PIC_MASTER_COMMAND, PIC_COMMAND_EOI);
}
