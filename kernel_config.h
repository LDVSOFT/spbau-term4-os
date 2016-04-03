#pragma once

//#define CONFIG_QEMU_GDB_HANG      /* infinite loop after long mode enabled */
#define CONFIG_TESTS

#define SERIAL_DIVISOR 0x0001u /* Serial port freq divisor */

#define PIT_DIVISOR  40000u     /* PIT freq divisor */
#define PIT_TICKS    3          /* PIT ticks for actions */
