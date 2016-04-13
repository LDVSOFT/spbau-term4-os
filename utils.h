#pragma once

#include <stdint.h>

static int min(int a, int b) {
	return (a < b) ? a : b;
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
	return (a < b) ? a : b;
}

static int max(int a, int b) {
	return (a > b) ? a : b;
}

static uint64_t max_u64(uint64_t a, uint64_t b) {
	return (a > b) ? a : b;
}
