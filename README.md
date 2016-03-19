# spbau-term4-os

## Building

0. `Makefile` -- from upstream. Added run-related tasks.
0. `kernel.ld` -- from upstream, linker script.
0. `bootstrap.S` -- from upstream, startup.
0. `kernel_config.h` -- from upstream, config.

## System

0. `interrupt.h`, `interrupt.c` -- from upstream, interrupts stuff (IDT & descriptors).
0. `interrupt-wrappers.S` -- Wrappers for interruption handling in C.
0. `pic.h`, `pic.c` -- PIC utils: init & EOI routines.
0. `pit.h`, `pit.c` -- PIT utils: init & interruption handler.
0. `ioport.h` -- from upstream, io C wrappers.
0. `serial.h`, `serial.c` -- Serial port utils: init, `putch` & `puts`.
0. `videomem.S` -- from upstream, VGA utils.
0. `main.c` -- from upstream, main function. Currecntly setups all stuff (PIC, PIT, Memory, IDT & Serial -- all here).
0. `multiboot.h` -- defines multiboot info struct

## Memory

0. `memory.h`, `memory.c` -- from upstream, memory stuff.
0. `bootstrap-alloc.c`, `bootstrap-alloc.h` -- bootstrap allocator for buddy allocator.
0. `buddy.h`, `buddy.c` -- Buddy allocator.
0. `slab-allocator.h`, `slab-allocator.c` -- SLAB allocator.
0. `paging.h`, `paging.c` -- from upstream (WITH PATCHED `pte_phys`), paging utils.
0. `page_descr.h` -- page desription struct, Buddy stores these for stuff (i.e. SLAB owning that page)

## Output
0. `print.h`, `print.c` -- `v?s?printf` functions, implemented via `ovprintf` that takes `strcut printer*` and executes pointer-passed handler
0. `log.h`, `log.c` -- high-level output for logging messages and errors (halting too).
