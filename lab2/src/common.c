#include "common.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

static const char *const METHOD_NAMES[M_COUNT] = {
    [M_MMAP_FILE] = "mmap_file",
    [M_MMAP_ANON] = "mmap_anon",
    [M_SHM]       = "shm",
    [M_FILE]      = "file",
    [M_PIPE]      = "pipe",
    [M_FIFO]      = "fifo",
    [M_SOCKET]    = "socket",
    [M_MQUEUE]    = "mqueue",
};

static const char *const TEST_NAMES[] = {
    [T_THROUGHPUT] = "throughput",
    [T_LATENCY]    = "latency",
    [T_CAPACITY]   = "capacity",
};

const char *method_name(method_t m) {
    return (m >= 0 && m < M_COUNT) ? METHOD_NAMES[m] : "unknown";
}

const char *test_name(test_t t) {
    return (t >= T_THROUGHPUT && t <= T_CAPACITY) ? TEST_NAMES[t] : "unknown";
}

int parse_method(const char *s, method_t *out) {
    for (int i = 0; i < M_COUNT; i++) {
        if (strcmp(s, METHOD_NAMES[i]) == 0) { *out = (method_t)i; return 0; }
    }
    return -1;
}

int parse_test(const char *s, test_t *out) {
    for (int i = 0; i <= T_CAPACITY; i++) {
        if (strcmp(s, TEST_NAMES[i]) == 0) { *out = (test_t)i; return 0; }
    }
    return -1;
}

ssize_t write_full(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)len;
}

ssize_t read_full(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = read(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;  /* unexpected EOF */
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)len;
}
