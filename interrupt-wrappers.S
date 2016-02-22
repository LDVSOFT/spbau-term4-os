#include "interrupt.h"

	.code64
	.global pit_handler_wrapper
	.extern pit_handler

pit_handler_wrapper:
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11

	cld
	call pit_handler

	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rax

	iretq
