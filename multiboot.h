#pragma once

#include "memory.h"
#include <stdint.h>

extern uint32_t mboot_info;

struct mboot_info {
	uint32_t flags;
	#define MBOOT_INFO_MEM_REGIONS      0
	uint32_t mem_lower;
	uint32_t mem_upper;
	#define MBOOT_INFO_BOOT_DEVICE      1
	uint32_t boot_device;
	#define MBOOT_INFO_CMDLINE          2
	uint32_t cmdline;
	#define MBOOT_INFO_MODS             3
	uint32_t mods_count;
	uint32_t mods_addr;
	union {
		#define MBOOT_INFO_SYMBOLS_AOUT 4
		struct {
			uint32_t tabsize;
			uint32_t strsize;
			uint32_t addr;
			uint32_t reserved;
		} __attribute__((packed)) aout;
		#define MBOOT_INFO_SYMBOLS_ELF  5
		struct {
			uint32_t num;
			uint32_t size;
			uint32_t addr;
			uint32_t shndx;
		} __attribute__((packed)) elf;
	} syms;
	#define MBOOT_INFO_MMAP             6
	uint32_t mmap_length;
	uint32_t mmap_addr;
	#define MBOOT_INFO_DRIVES           7
	uint32_t drives_length;
	uint32_t drives_addr;
	#define MBOOT_INFO_CONFIG           8
	uint32_t config_table;
	#define MBOOT_INFO_BOOTLOADER       9
	uint32_t boot_loader_name;
	#define MBOOT_INFO_APM              10
	uint32_t apm_table;
	#define MBOOT_INFO_VIDEO            11
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;
} __attribute__((packed));

struct mboot_info_mod {
	uint32_t mod_start;
	uint32_t mod_end;
	uint32_t string;
	uint32_t reserved;
} __attribute__((packed));

static struct mboot_info* mboot_info_get(void) {
	//Thanks to linker, just access `mboot_info' will access it via low memoty.
	//So, first we need to jump there via high memory, and them read the freaking pointer.
	return (struct mboot_info*) va(*(uint64_t*) va((uint64_t) &mboot_info));
}
