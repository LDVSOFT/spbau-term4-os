#pragma once

#include <stdarg.h>

int vprintf(const char *format, va_list args);
int printf(const char *format, ...);
int vsprintf(char *s, const char *format, va_list args);
int sprintf(char *s, const char *format, ...);
