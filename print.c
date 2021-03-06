#include "print.h"
#include "serial.h"
#include "string.h"

#include <stddef.h>
#include <stdbool.h>

struct printer;

typedef void (*printer_print_t)(struct printer* self, char c);
typedef void (*printer_end_t)(struct printer* self);

struct printer {
	printer_print_t print;
	printer_end_t end;
};

static void __printer_end(struct printer* printer) {
}

static inline void printer_print(struct printer *printer, char c) {
	printer->print(printer, c);
}

static inline void printer_end(struct printer *printer) {
	printer->end(printer);
}

#define DSIZE_SIZE_T 10
#define NUMBER_BUFFER_SIZE 65

static int ovprintf_print_number_(
		uintmax_t value, struct printer *printer,
		bool isUpper, int base, int width, bool addSeps, bool zeroFill) {
	int count = 0;
	char buffer[NUMBER_BUFFER_SIZE];
	int buffer_pos = 0;

	while (buffer_pos == 0 || value > 0 || width > 0) {
		--width;
		int digit = value % base;
		if (value == 0 && buffer_pos != 0) {
			buffer[buffer_pos++] = zeroFill ? '0' : ' ';
		} else if (digit < 10) {
			buffer[buffer_pos++] = '0' + digit;
		} else if (isUpper) {
			buffer[buffer_pos++] = 'A' + digit - 10;
		} else {
			buffer[buffer_pos++] = 'a' + digit - 10;
		}
		value /= base;
	}
	while (buffer_pos > 0) {
		printer_print(printer, buffer[--buffer_pos]);
		count += 1;
		if (addSeps) {
			if (buffer_pos % 4 == 0 && buffer_pos > 0) {
				count += 1;
				printer_print(printer, ':');
			}
		}
	}

	return count;
}

static int ovprintf_print_number(
		va_list args, struct printer *printer,
		int dSize, bool isUnsigned, bool isUpper, int base, int width, bool addSeps, bool zeroFill) {
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
			default:
				return 0;
		}
		if (data < 0) {
			printer_print(printer, '-');
			count += 1;
			data *= -1;
		}
		count += ovprintf_print_number_(data, printer, isUpper, base, width, addSeps, zeroFill);
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
			default:
				return 0;
		}
		count += ovprintf_print_number_(data, printer, isUpper, base, width, addSeps, zeroFill);
	}

	return count;
}

static int ovprintf_print_string(va_list args, struct printer *printer, int dSize, int width) {
	int count = 0;

	switch (dSize) {
		case 0:
			{
				const char *str = va_arg(args, const char*);
				int len = strlen(str);
				while (width > len) {
					width--;
					printer_print(printer, ' ');
				}
				for ( ; *str != 0; ++str) {
					printer_print(printer, *str);
					++count;
				}
			}
			break;
		// NOPE, I'M NOT GOING TO ADD WIDESTRINGS SUPPORT NOW
		default:
			return 0;
	}

	return count;
}

static int ovprintf_print_char(va_list args, struct printer *printer, int dSize) {
	switch (dSize) {
		case 0:	{
				int c = va_arg(args, int);
				printer_print(printer, c);
			}
			return 1;
		// NOPE, I'M NOT GOING TO ADD WIDECHARS SUPPORT NOW
		default:
			return 0;
	}
}

static int ovprintf(const char *format, va_list args, struct printer *printer) {
	int count = 0;
	for (; *format != 0; ++format) {
		if (*format != '%') {
			printer_print(printer, *format);
			++count;
		} else {
			bool isFirst = true;
			bool isOver = false;
			int dSize = 0;
			bool zeroFill = false;
			int width = 0;
			for (++format; *format != 0 && !isOver; isFirst = 0) {
				switch (*format) {
					// %
					case '%':
						printer_print(printer, '%');
						++count;
						isOver = true;
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
					// View modifiers (0, width)
					case '0':
						if (isFirst) {
							zeroFill = true;
							break;
						}
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						width = width * 10 + *format - '0';
						break;
					// Char (c)
					case 'c':
						count += ovprintf_print_char(args, printer, dSize);
						isOver = true;
						break;
					// String (s)
					case 's':
						count += ovprintf_print_string(args, printer, dSize, width);
						isOver = true;
						break;
					// Number (diuoxX)
					case 'd':
					case 'i':
						count += ovprintf_print_number(args, printer, dSize, false, false, 10, width, false, zeroFill);
						isOver = true;
						break;
					case 'u':
						count += ovprintf_print_number(args, printer, dSize, true , false, 10, width, false, zeroFill);
						isOver = true;
						break;
					case 'o':
						count += ovprintf_print_number(args, printer, dSize, true , false, 8 , width, false, zeroFill);
						isOver = true;
						break;
					case 'x':
						count += ovprintf_print_number(args, printer, dSize, true , false, 16, width, false, zeroFill);
						isOver = true;
						break;
					case 'X':
						count += ovprintf_print_number(args, printer, dSize, true , true , 16, width, false, zeroFill);
						isOver = true;
						break;
					// Pointer (p)
					case 'p':
						count += ovprintf_print_number(args, printer, DSIZE_SIZE_T, true, true, 16, sizeof(size_t) * 2, true, true);
						isOver = true;
					// Else
					default:
						isOver = true;
						break;
				}
				if (!isOver) {
					++format;
				}
			}
		}
	}
	printer_end(printer);
	return count;
}

struct serial_printer {
	struct printer super;
};

static void __serial_print(struct serial_printer *printer, char c) {
	serial_putch(c);
}

static void serial_printer_init(struct serial_printer *self) {
	self->super.print = (printer_print_t) __serial_print;
	self->super.end = __printer_end;
}

struct buffer_printer {
	struct printer super;
	char *buffer;
	size_t left;
};

static void __buffer_print(struct buffer_printer *self, char c) {
	if (self->left != 0) {
		*(self->buffer) = c;
		self->buffer++;
		--self->left;
	}
}

static void __buffer_end(struct buffer_printer *self) {
	*(self->buffer) = 0;
}

static void buffer_printer_init(struct buffer_printer *self, char *buffer, size_t size) {
	self->super.print = (printer_print_t) __buffer_print;
	self->super.end = (printer_end_t) __buffer_end;
	self->buffer = buffer;
	self->left = size - 1; // 1 is left for EOL
}

int vprintf(const char *format, va_list args) {
	struct serial_printer printer;
	serial_printer_init(&printer);
	return ovprintf(format, args, (struct printer*) &printer);
}

int printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	int result = vprintf(format, args);
	va_end(args);
	return result;
}

int vsnprintf(char *s, size_t n, const char *format, va_list args) {
	struct buffer_printer printer;
	buffer_printer_init(&printer, s, n);
	return ovprintf(format, args, (struct printer*) &printer);
}

int vsprintf(char *s, const char *format, va_list args) {
	return vsnprintf(s, 0, format, args);
}

int snprintf(char *s, size_t n, const char *format, ...) {
	va_list args;
	va_start(args, format);
	int result = vsnprintf(s, n, format, args);
	va_end(args);
	return result;
}

int sprintf(char *s, const char *format, ...) {
	va_list args;
	va_start(args, format);
	int result = vsprintf(s, format, args);
	va_end(args);
	return result;
}
