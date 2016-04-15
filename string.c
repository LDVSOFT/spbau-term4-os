#include "string.h"

int strlen(const char* s) {
	int i = 0;
	for (; *s; ++i, ++s) ;
	return i;
}

int strncmp(const char* a, const char* b, unsigned int n) {
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

int strcmp(const char* a, const char* b) {
	return strncmp(a, b, -1);
}

char* strncpy(char* dst, const char* src, int n) {
	--n;
	while (n > 0 && *src) {
		*dst = *src;
		--n;
		++src;
		++dst;
	}
	*dst = 0;
	return dst;
}

void memcpy(void* dst, const void* src, uint64_t size) {
	char* dst_p = (char*) dst;
	const char* src_p = (const char*) src;

	uint64_t longs = size / sizeof(uint64_t);
	while (longs --> 0) {
		*(uint64_t*) dst_p = *(uint64_t*) src_p;
		dst_p += sizeof(uint64_t);
		src_p += sizeof(uint64_t);
		size -= sizeof(uint64_t);
	}
	while (size --> 0) {
		*dst_p = *src_p;
		dst_p++;
		src_p++;
	}
}
