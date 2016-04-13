#pragma once

#include <stdint.h>

int strlen(const char* s);
int strncmp(const char* a, const char* b, int n);
char* strncpy(char* dst, const char* src, int n);
void memcpy(void* dst, const void* src, uint64_t size);
