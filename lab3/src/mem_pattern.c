#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "../../shared/timer.h"

#define SEQ_REPEATS 4
#define RANDOM_ACCESSES (4u * 1000u * 1000u)

static volatile uint64_t g_sink;

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static int run_size(size_t bytes, const char *label) {
    size_t n = bytes / sizeof(uint64_t);
    uint64_t *buf = xaligned_alloc(64, n * sizeof(uint64_t));
    uint64_t *idx = malloc(RANDOM_ACCESSES * sizeof(uint64_t));
    if (!idx) {
        fprintf(stderr, "index allocation failed\n");
        free(buf);
        return 1;
    }

    for (size_t i = 0; i < n; i++) {
        buf[i] = i;
    }

    uint64_t state = 0x9E3779B97F4A7C15ull ^ (uint64_t)bytes;
    for (size_t i = 0; i < RANDOM_ACCESSES; i++) {
        idx[i] = rng_next(&state) & (n - 1);
    }

    uint64_t seq_acc = 0;
    double t0 = now_sec();
    for (int r = 0; r < SEQ_REPEATS; r++) {
        for (size_t i = 0; i < n; i++) {
            seq_acc += buf[i];
        }
    }
    double seq_ns = (now_sec() - t0) * 1e9 / (double)(SEQ_REPEATS * n);
    g_sink ^= seq_acc;

    uint64_t rnd_acc = 0;
    t0 = now_sec();
    for (size_t i = 0; i < RANDOM_ACCESSES; i++) {
        rnd_acc += buf[idx[i]];
    }
    double rnd_ns = (now_sec() - t0) * 1e9 / (double)RANDOM_ACCESSES;
    g_sink ^= rnd_acc;

    char seq_param[64], rnd_param[64];
    snprintf(seq_param, sizeof(seq_param), "sequential_%s", label);
    snprintf(rnd_param, sizeof(rnd_param), "random_%s", label);
    emit("mem", seq_param, seq_ns, "ns_per_access");
    emit("mem", rnd_param, rnd_ns, "ns_per_access");
    emit("mem", rnd_param, seq_ns > 0 ? rnd_ns / seq_ns : 0.0, "slowdown_vs_seq");

    free(idx);
    free(buf);
    return 0;
}

int bench_mem_pattern(void) {
    static const struct {
        size_t bytes;
        const char *label;
    } levels[] = {
        {32u * 1024u, "l1_32KB"},
        {4u * 1024u * 1024u, "l2_4MB"},
        {64u * 1024u * 1024u, "dram_64MB"},
    };

    int rc = 0;
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        rc |= run_size(levels[i].bytes, levels[i].label);
    }
    return rc;
}
