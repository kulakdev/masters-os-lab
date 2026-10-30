#pragma once
#include <stddef.h>
#include <stdint.h>

int bench_mem_pattern(void);
int bench_stride(void);
int bench_latency(void);
int bench_matmul(void);
int bench_false_sharing(void);
int bench_race(void);

uint64_t cache_line_size(void);
void emit(const char *bench, const char *param, double value, const char *unit);
void *xaligned_alloc(size_t alignment, size_t size);
