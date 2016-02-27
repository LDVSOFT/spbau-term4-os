#pragma once

#include <stdarg.h>
#include <stddef.h>

int vprintf(const char *format, va_list args);
int printf(const char *format, ...);
int vsprintf(char *s, const char *format, va_list args);
int sprintf(char *s, const char *format, ...);
int vsnprintf(char *s, size_t n, const char *format, va_list args);
int snprintf(char *s, size_t n, const char *format, ...);
