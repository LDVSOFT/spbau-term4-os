#pragma once

#define INTERRUPT_COUNT 0x100

#define INTERRUPT_PIC_MASTER 0x20
#define INTERRUPT_PIC_SLAVE  0x28

#define INTERRUPT_PIT ((uint8_t)INTERRUPT_PIC_MASTER + 0)

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

struct interrupt_info {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rsi, rdi, rdx, rcx, rbx, rax;
	uint64_t rsp, id, error, rip, cs, rflags;
} __attribute__((packed));

typedef struct interrupt idt_t[INTERRUPT_COUNT];

typedef void (*interrupt_handler_wrapper_t)(void);
typedef void (*interrupt_handler_t)(struct interrupt_info* info);

void interrupt_init(void);
void interrupt_set(uint8_t id, interrupt_handler_t handler);
void interrupt_handler_halt(struct interrupt_info* info);

extern interrupt_handler_wrapper_t interrupt_handler_wrappers[INTERRUPT_COUNT];

static inline void interrupt_set_idt(const struct idt_ptr *idt_ptr)
{ asm volatile ("lidt (%0)" : : "a"(idt_ptr)); }

static inline void interrupt_enable(void)
{ asm volatile ("sti"); }

static inline void interrupt_disable(void)
{ asm volatile ("cli"); }

static inline void hlt(void)
{ asm volatile ("hlt"); }

#endif /*__ASM_FILE__*/
