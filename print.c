#include "print.h"
#include "serial.h"

#include <stddef.h>

typedef void (*print_char_f)(char c, void *buf_ptr);

#define DSIZE_SIZE_T 10
#define NUMBER_BUFFER_SIZE 65

static int ovprintf_print_number_(
		uintmax_t value, print_char_f printer, void *printer_data,
		int isUpper, int base, int width, int addSeps) {
	int count = 0;
	char buffer[NUMBER_BUFFER_SIZE];
	int buffer_pos = 0;

	while (buffer_pos == 0 || value > 0 || width > 0) {
		--width;
		int digit = value % base;
		value /= base;
		if (digit < 10) {
			buffer[buffer_pos++] = '0' + digit;
		} else if (isUpper) {
			buffer[buffer_pos++] = 'A' + digit - 10;
		} else {
			buffer[buffer_pos++] = 'a' + digit - 10;
		}
	}
	while (buffer_pos > 0) {
		printer(buffer[--buffer_pos], printer_data);
		count += 1;
		if (addSeps) {
			if (buffer_pos % 4 == 0 && buffer_pos > 0) {
				count += 1;
				printer(':', printer_data);
			}
		}
	}

	return count;
}

static int ovprintf_print_number(
		va_list args, print_char_f printer, void *printer_data,
		int dSize, int isUnsigned, int isUpper, int base, int width, int addSeps) {
	//serial_puts("\n\nENTERED NUMBER\n\n");
	int count = 0;
	if (!isUnsigned) {
		intmax_t data;
		switch (dSize) {
			case -2: // char and short are promoted to int
			case -1: // So just read int
			case 0:
				data = va_arg(args, signed int);
				break;
			case 1:
				data = va_arg(args, signed long int);
				break;
			case 2:
				data = va_arg(args, signed long long int);
				break;
		}
		if (data < 0) {
			printer('-', printer_data);
			count += 1;
			data *= -1;
		}
		count += ovprintf_print_number_(data, printer, printer_data, isUpper, base, width, addSeps); 
	} else {
		uintmax_t data;
		switch (dSize) {
			case -2: // char and short are promoted to int
			case -1: // So just read int
			case 0:
				data = va_arg(args, unsigned int);
				break;
			case 1:
				data = va_arg(args, unsigned long int);
				break;
			case 2:
				data = va_arg(args, unsigned long long int);
				break;
			case DSIZE_SIZE_T:
				data = va_arg(args, size_t);
				break;
		}
		count += ovprintf_print_number_(data, printer, printer_data, isUpper, base, width, addSeps);
	}

	return count;
}

static int ovprintf_print_string(va_list args, print_char_f printer, void *printer_data, int dSize) {
	int count = 0;

	switch (dSize) {
		case 0:
			for (const char *str = va_arg(args, char *); *str != 0; ++str) {
				printer(*str, printer_data);
				++count;
			}
			break;
		// NOPE, I'M NOT GOING TO ADD WIDESTRINGS SUPPORT NOW
		default:
			return 0;
	}

	return count;
}

static int ovprintf_print_char(va_list args, print_char_f printer, void *printer_data, int dSize) {
	switch (dSize) {
		case 0:	{
				int c = va_arg(args, int);
				printer(c, printer_data);
			}
			return 1;
		// NOPE, I'M NOT GOING TO ADD WIDECHARS SUPPORT NOW
		default:
			return 0;
	}
}

static int ovprintf(const char *format, va_list args, print_char_f printer, void *printer_data) {
	int count = 0;
	for (; *format != 0; ++format) {
		if (*format != '%') {
			printer(*format, printer_data);
			++count;
		} else {
			int isOver = 0;
			int dSize = 0;
			for (++format; *format != 0 && !isOver; ) {
				switch (*format) {
					// %
					case '%':
						printer('%', printer_data);
						++count;
						isOver = 1;
						break;
					// Size modifiers (hlz)
					case 'h':
						--dSize;
						break;
					case 'l':
						++dSize;
						break;
					case 'z':
						dSize = DSIZE_SIZE_T;
						break;
					// Char (c)
					case 'c':
						count += ovprintf_print_char(args, printer, printer_data, dSize);
						isOver = 1;
						break;
					// String (s)
					case 's':
						count += ovprintf_print_string(args, printer, printer_data, dSize);
						isOver = 1;
						break;
					// Number (diuoxX)
					case 'd':
					case 'i':
						count += ovprintf_print_number(args, printer, printer_data, dSize, 0, 0, 10, 0, 0);
						isOver = 1;
						break;
					case 'u':
						count += ovprintf_print_number(args, printer, printer_data, dSize, 1, 0, 10, 0, 0);
						isOver = 1;
						break;
					case 'o':
						count += ovprintf_print_number(args, printer, printer_data, dSize, 1, 0, 8, 0, 0);
						isOver = 1;
						break;
					case 'x':
						count += ovprintf_print_number(args, printer, printer_data, dSize, 1, 0, 16, 0, 0);
						isOver = 1;
						break;
					case 'X':
						count += ovprintf_print_number(args, printer, printer_data, dSize, 1, 1, 16, 0, 0);
						isOver = 1;
					// Pointer (p)
					case 'p':
						count += ovprintf_print_number(args, printer, printer_data, DSIZE_SIZE_T, 1, 1, 16, sizeof(size_t) * 2, 1);
						isOver = 1;
					// Else
					default:
						isOver = 1;
						break;
				}
				if (!isOver) {
					++format;
				}
			}
		}
	}
	return count;
}

static void serial_printer(char c, void *data __attribute__((unused))) {
	serial_putch(c);
}

static void buffer_printer(char c, void *data) {
	char **buffer_ptr = (char **) data;
	**buffer_ptr = c;
	*buffer_ptr = *buffer_ptr + 1;
}

int vprintf(const char *format, va_list args) {
	return ovprintf(format, args, &serial_printer ,0);
}

int printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	int result = vprintf(format, args);
	va_end(args);
	return result;
}

int vsprintf(char *s, const char *format, va_list args) {
	return ovprintf(format, args, &buffer_printer, &s);
}

int sprintf(char *s, const char *format, ...) {
	va_list args;
	va_start(args, format);
	int result = vsprintf(s, format, args);
	va_end(args);
	return result;
}
