#include "interrupt.h"
#include "memory.h"
#include "log.h"
#include "threads.h"

#include <stddef.h>

#define INTERRUPT_FLAG_INT64  0b1110
#define INTERRUPT_FLAG_TRAP64 0b1111
#define INTERRUPT_FLAG_PRESENT 0b10000000

static struct idt_ptr idt_ptr;
static idt_t idt;

void interrupt_init(void) {
	idt_ptr.size = sizeof(idt) - 1;
	idt_ptr.base = (uint64_t)idt;
	for (int i = 0; i != INTERRUPT_COUNT; ++i) {
		struct interrupt *interrupt = &idt[i];
		uint64_t handler = (uint64_t)interrupt_handler_wrappers[i];
		interrupt->offset_low    = get_bits(handler, 0, 16);
		interrupt->offset_medium = get_bits(handler, 16, 16);
		interrupt->offset_high   = get_bits(handler, 32, 32);
		interrupt->ist = 0;
		interrupt->segment_selector = KERNEL_CODE;
		interrupt->flags = INTERRUPT_FLAG_INT64 | INTERRUPT_FLAG_PRESENT;
		interrupt->reserved = 0;
	}
	interrupt_set_idt(&idt_ptr);

	interrupt_set(6 , interrupt_handler_halt);
	interrupt_set(8 , interrupt_handler_halt);
	interrupt_set(13, interrupt_handler_halt);
	interrupt_set(14, interrupt_handler_halt);
}

static interrupt_handler_t handlers[INTERRUPT_COUNT];

void interrupt_set(uint8_t id, interrupt_handler_t handler) {
	handlers[id] = handler;
}

void interrupt_handler(struct interrupt_info* info) {
	int id = info->id;
	log(LEVEL_VVV, "INT %d...", info->id);
	if (handlers[id] == NULL) {
		log(LEVEL_WARN, "No handler for INT %u (error no %u)!", info->id, info->error);
	} else {
		handlers[id](info);
	}
}

const char* interrupt_message(int id) {
	switch (id) {
		case 0:  return "Division error";
		case 1:  return "Debug exception";
		case 2:  return "NMI";
		case 3:  return "Breakpoint";
		case 4:  return "Overflow";
		case 5:  return "BOUND range expection";
		case 6:  return "Invalid opcode";
		case 7:  return "No math soprocessor";
		case 8:  return "Double fault";
		case 9:  return "Coprocessor segment overrun";
		case 10: return "Invalid TSS";
		case 11: return "Segment not present";
		case 12: return "Stack segment fault";
		case 13: return "General protection";
		case 14: return "Page fault";
		case 16: return "x87 FPU error";
		case 17: return "Alignment check";
		case 18: return "Machine check";
		case 19: return "SIMD floating point error";
		case 20: return "Virtualization exception";
		default: return "<unknown>";
	}
}

static void backtrace(uint64_t rbp, uintptr_t stack_begin, uintptr_t stack_end)
{
	int depth = 0;
	while (rbp >= stack_begin && rbp < stack_end - 2 * sizeof(uint64_t)) {
		uint64_t* stack_ptr = (uint64_t*) rbp;
		log(LEVEL_ERROR, "%2d: %p", depth++, (void *)stack_ptr[1]);
		rbp = stack_ptr[0];
	}
}

void interrupt_handler_halt(struct interrupt_info* info) {
	log(LEVEL_ERROR, "Interruption %u: %s. Error code %u.", info->id, interrupt_message(info->id), info->error);
	#define log_r(x) log(LEVEL_ERROR, "%s=%p", #x, info->x)
	log_r(rip); log_r(cs ); log_r(rflags);
	log_r(rax); log_r(rbx); log_r(rcx); log_r(rdx);
	log_r(rdi); log_r(rsi); log_r(rbp); log_r(rsp);
	log_r(r8 ); log_r(r9 ); log_r(r10); log_r(r11);
	log_r(r12); log_r(r13); log_r(r14); log_r(r15);
	uint64_t tmp;
	asm volatile ("movq %%cr2, %0" : "=a"(tmp));
	log(LEVEL_ERROR, "cr2=%p", tmp);

	struct thread* current = thread_current();
	log(LEVEL_ERROR, "Thread=%p (name=\"%s\").", current, current ? current->name : "<NULL>");
	backtrace(info->rbp, (uint64_t)current->stack, (uint64_t)current->stack + THREAD_STACK_SIZE);

	halt("UNEXPECTED EXCEPTION");
}
