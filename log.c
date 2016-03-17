#include "log.h"
#include "print.h"
#include "interrupt.h"
#include <stdarg.h>
#include <stdbool.h>

static const char* color_reset = "\e[0m";

static const char* colors[_LEVEL_MAX] = {0};
static int log_level = LEVEL_INFO;

void log_set_color_enabled(bool color_enabled) {
	if (color_enabled) {
		colors[_LEVEL_MIN]  = "\e[30m";
		colors[LEVEL_V]     = "\e[37m";
		colors[LEVEL_LOG]   = "\e[38m";
		colors[LEVEL_INFO]  = "\e[1;38m";
		colors[LEVEL_WARN]  = "\e[1;33m";
		colors[LEVEL_ERROR] = "\e[1;31m";
		colors[LEVEL_FAULT] = "\e[1;37;41m";
	} else {
		for (int i = _LEVEL_MIN; i != _LEVEL_MAX; ++i) {
			colors[i] = 0;
		}
	}
}

void log_set_level(int level) {
	log_level = level;
}

static const char* log_get_color(int level) {
	for (int i = level; i >= _LEVEL_MIN; --i) {
		if (colors[i]) {
			return colors[i];
		}
	}
	return NULL;
}

void vlog_tagged(int level, const char *tag, const char* format, va_list args) {
	if (level < log_level) {
		return;
	}
	const char* level_color = log_get_color(level);
	printf("!%s[%02d %s] ", level_color ?: "", level, tag);
	vprintf(format, args);
	printf("%s\n", level_color ? color_reset : "");
}

void log_tagged(int level, const char *tag, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vlog_tagged(level, tag, format, args);
	va_end(args);
}

void halt_tagged(const char* tag, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vlog_tagged(LEVEL_FAULT, tag, format, args);
	va_end(args);
	interrupt_disable();
	while (true) {
		hlt();
	}
}
