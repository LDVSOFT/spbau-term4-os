#include "string.h"

int strlen(const char* s) {
	int i = 0;
	for (; *s; ++i, ++s) ;
	return i;
}

int strncmp(const char* a, const char* b, int n) {
	while (n > 0 && *a == *b && *a) {
		--n;
		++a;
		++b;
	}
	if (n == 0) {
		return 0;
	}
	if (*a < *b) {
		return -1;
	}
	if (*a > *b) {
		return 1;
	}
	return 0;
}
