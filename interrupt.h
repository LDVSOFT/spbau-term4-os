#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>

#define INTERRUPT_PIC_MASTER 0x20
#define INTERRUPT_PIC_SLAVE  0x28

struct idt_ptr {
	uint16_t size;
	uint64_t base;
} __attribute__((packed));

static inline void set_idt(const struct idt_ptr *ptr)
{ __asm__ volatile ("lidt (%0)" : : "a"(ptr)); }

#endif /*__INTERRUPT_H__*/
