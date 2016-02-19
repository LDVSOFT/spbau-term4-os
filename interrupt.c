#include "interrupt.h"
#include "memory.h"

#define INTERRUPT_FLAG_INT64  0b1110
#define INTERRUPT_FLAG_TRAP64 0b1111
#define INTERRUPT_FLAG_PRESENT 0b10000000

void interrupt_init_ptr(struct idt_ptr *ptr, idt_t table) {
	ptr->base = (uint64_t) table;
	ptr->size = sizeof(idt_t) - 1;
}

void interrupt_set(const struct idt_ptr *ptr, uint8_t id, uint64_t handler) {
	struct interrupt *interrupt = &((struct interrupt *) ptr->base)[id];
	interrupt->offset_low    = (handler >>  0) & 0xFFFF;
	interrupt->offset_medium = (handler >> 16) & 0xFFFF;
	interrupt->offset_high   = (handler >> 32) & 0xFFFFFFFF;
	interrupt->ist = 0;
	interrupt->segment_selector = KERNEL_CODE;
	interrupt->flags = INTERRUPT_FLAG_INT64 | INTERRUPT_FLAG_PRESENT;
	interrupt->reserved = 0;
}
