# spbau-term4-os

1. `Makefile` -- from upstream. Added run & run-debug tasks.
2. `kernel.ld` -- from upstream, linker script.
3. `bootstrap.S` -- from upstream, startup.
4. `interrupt.h` -- from upstream, interrupts stuff (IDT & descriptors).
5. `ioport.h` -- from upstream, io C wrappers.
6. `kernel_config.h` -- from upstream, config. Frequency setup goes here.
7. `main.c` -- from upstream, main function. Currecntly setups all stuff (PIC, PIT, IDT & Serial -- all here).
8. `memory.h` -- from upstream, memory stuff.
9. `videomem.S` -- from upstream, VGA utils.
10. `pic.h`, `pic.c` -- PIC utils: init & EOI routines.
11. `pit.h`, `pit.c` -- PIT utils: init & interruption handler.
12. `serial.h`, `serial.c` -- Serial port utils: init, `putch` & `puts`.
13. `print.h`, `print.c` -- `v?s?printf` functions, implemented via `ovprintf` that takes `printer` function & pointer to it's data as arguments. Currently supports only `%s`, `%c`, and number formats (without width/precision, though).
14. `interrupt-wrappers.S` -- Wrappers for interruption handling in C.
