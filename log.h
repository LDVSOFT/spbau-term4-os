#pragma once

#include <stdarg.h>
#include <stdbool.h>

enum level {
	_LEVEL_MIN  =  0,
	LEVEL_VVV   =  1,
	LEVEL_VV    = 10,
	LEVEL_V     = 20,
	LEVEL_LOG   = 30,
	LEVEL_INFO  = 40,
	LEVEL_WARN  = 50,
	LEVEL_ERROR = 60,
	LEVEL_FAULT = 70,
	_LEVEL_MAX  = 100,
};

void log_set_level(int level);
void log_set_color_enabled(bool color_enabled);
void vlog_tagged(int level, const char* tag, const char* format, va_list args);
void log_tagged(int level, const char* tag, const char* format, ...);

void halt_tagged(const char* tag, const char* format, ...);

#define log(level, ...) log_tagged(level, __func__, __VA_ARGS__)
#define halt(...)       halt_tagged(__func__, __VA_ARGS__)
