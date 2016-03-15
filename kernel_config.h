#pragma once

//#define CONFIG_QEMU_GDB_HANG      /* infinite loop after long mode enabled */

#define LOG_LEVEL LEVEL_ERROR
#define LOG_COLOR 0

#define SERIAL_DIVISOR 0x0001u /* Serial port freq divisor */

#define PIT_DIVISOR  40000u     /* PIT freq divisor */
#define PIT_TICKS    30         /* PIT ticks for actions */
