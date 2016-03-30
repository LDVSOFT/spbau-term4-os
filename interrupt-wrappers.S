#include "interrupt.h"

	.code64
	.align 16
	.altmacro
	.extern interrupt_handler
	.global interrupt_handler_wrappers

.macro STORE_REGS
	push %rsp
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rdi
	push %rsi
	push %rbp
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
.endm

.macro RESTORE_REGS
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rbp
	pop %rsi
	pop %rdi
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	pop %rsp
.endm

interrupt_handler_wrapper:
	STORE_REGS
	mov %rsp, %rdi
	cld
	call interrupt_handler
	RESTORE_REGS
	add $16, %rsp // Error code & no
	iretq

.macro DEFINE_INTERRUPT_HANDLER id
interrupt_handler_wrapper_\id:
	pushq $0
	pushq $\id
	jmp interrupt_handler_wrapper
.endm

.macro DEFINE_INTERRUPT_HANDLER_ERROR id
interrupt_handler_wrapper_\id:
	pushq $\id
	jmp interrupt_handler_wrapper
.endm

.macro FOREACH l r action
	.if \r-\l == 1
	action \l
	.else
	FOREACH l, %((\l+\r)/2), action
	FOREACH %((\l+\r)/2), r, action
	.endif
.endm

FOREACH  0,  8, DEFINE_INTERRUPT_HANDLER
FOREACH  8,  9, DEFINE_INTERRUPT_HANDLER_ERROR
FOREACH  9, 10, DEFINE_INTERRUPT_HANDLER
FOREACH 10, 15, DEFINE_INTERRUPT_HANDLER_ERROR
FOREACH 15, 17, DEFINE_INTERRUPT_HANDLER
FOREACH 17, 18, DEFINE_INTERRUPT_HANDLER_ERROR
FOREACH 18, INTERRUPT_COUNT, DEFINE_INTERRUPT_HANDLER

.macro DEFINE_INTERRUPT_HANDLER_POINTER id
	.quad interrupt_handler_wrapper_\id
.endm

interrupt_handler_wrappers:
	FOREACH 0, INTERRUPT_COUNT, DEFINE_INTERRUPT_HANDLER_POINTER
