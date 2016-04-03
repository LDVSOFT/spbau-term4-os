	.code64
	.global thread_switch
	.global thread_run_wrapper

thread_switch:
	pushfq
	push %rbp
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15

	movq %rsp, (%rdi)
	movq %rsi, %rsp

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	pop %rbp
	popfq

	ret

thread_run_wrapper:
	pop %rdi
	call thread_run
	// unreachable
	ret
