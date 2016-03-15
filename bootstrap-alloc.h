#pragma once

#include "stdint.h"
#include "memory.h"

#define BOOTSTRAP_MMAP_MAX_LENGTH 64

extern int bootstrap_mmap_length;
extern struct mmap_entry bootstrap_mmap[BOOTSTRAP_MMAP_MAX_LENGTH];

void bootstrap_init_mmap();
phys_t bootstrap_alloc(uint32_t size);

