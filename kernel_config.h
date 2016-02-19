#ifndef __KERNEL_CONFIG_H__
#define __KERNEL_CONFIG_H__

//#define CONFIG_QEMU_GDB_HANG      /* infinite loop after long mode enabled */

#define SERIAL_DIVISOR 0x0001u /* Serial port freq divisor */

#define PIT_DIVISOR  40000u     /* PIT freq divisor */
#define PIT_INTERVAL 1.0f       /* PIT interval for actions */

#endif /*__KERNEL_CONFIG_H__*/
