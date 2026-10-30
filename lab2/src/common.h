#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    M_MMAP_FILE, M_MMAP_ANON, M_SHM, M_FILE,
    M_PIPE, M_FIFO, M_SOCKET, M_MQUEUE, M_COUNT
} method_t;

typedef enum { T_THROUGHPUT, T_LATENCY, T_CAPACITY } test_t;

typedef struct {
    method_t method;
    test_t   test;
    size_t   msg_size;
    long     msg_count;
    int      fsync;
    int      csv;
} config_t;

typedef struct {
    double   duration_sec;
    double   latency_us;      /* one-way latency (RTT/2) */
    uint64_t bytes;
    long     msgs;
    uint64_t capacity_bytes;
} results_t;

/* common.c */
const char *method_name(method_t m);
const char *test_name(test_t t);
int         parse_method(const char *s, method_t *out);
int         parse_test(const char *s, test_t *out);
ssize_t     write_full(int fd, const void *buf, size_t len);
ssize_t     read_full(int fd, void *buf, size_t len);

/* bench modules: each is a self-contained fork()-based harness */
int bench_mmap(const config_t *cfg, results_t *out);   /* mmap_file + mmap_anon */
int bench_shm(const config_t *cfg, results_t *out);
int bench_file(const config_t *cfg, results_t *out);
int bench_pipe(const config_t *cfg, results_t *out);
int bench_fifo(const config_t *cfg, results_t *out);
int bench_socket(const config_t *cfg, results_t *out);
int bench_mqueue(const config_t *cfg, results_t *out);
