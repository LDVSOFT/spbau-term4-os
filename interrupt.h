#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#define INTERRUPT_COUNT 0x100

#define INTERRUPT_PIC_MASTER 0x20
#define INTERRUPT_PIC_SLAVE  0x28

#define INTERRUPT_PIT (INTERRUPT_PIC_MASTER + 0)

#ifndef __ASM_FILE__

#include <stdint.h>

struct idt_ptr {
	uint16_t size;
	uint64_t base;
} __attribute__((packed));

struct interrupt {
	uint16_t offset_low;
	uint16_t segment_selector;
	uint8_t  ist;
	uint8_t  flags;
	uint16_t offset_medium;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed));

typedef struct interrupt idt_t[INTERRUPT_COUNT];

static inline void set_idt(const struct idt_ptr *idt_ptr)
{ __asm__ volatile ("lidt (%0)" : : "a"(idt_ptr)); }

// Init IDT pointer
void interrupt_init_ptr(struct idt_ptr *idt_ptr, idt_t table);

// Sets interruption handler for interrupt #id in given IDT
void interrupt_set(const struct idt_ptr *idt_ptr, uint8_t id, uint64_t handler);

// Handlers
// PIT handler (wrapper in assemby):
void interrupt_pit_handler_wrapper(void);
void interrupt_pit_handler(void);

#endif /*__ASM_FILE__*/

#endif /*__INTERRUPT_H__*/
