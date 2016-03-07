#include "interrupt.h"

	.code64
	.global pit_handler_wrapper
	.extern pit_handler

#define STORE_REGS \
	push %rax ; \
	push %rcx ; \
	push %rdx ; \
	push %rsi ; \
	push %rdi ; \
	push %r8  ; \
	push %r9  ; \
	push %r10 ; \
	push %r11 ;

#define RESTORE_REGS \
	pop %r11 ; \
	pop %r10 ; \
	pop %r9  ; \
	pop %r8  ; \
	pop %rdi ; \
	pop %rsi ; \
	pop %rdx ; \
	pop %rcx ; \
	pop %rax ;

pit_handler_wrapper:
	STORE_REGS
	cld
	call pit_handler
	RESTORE_REGS
	iretq
