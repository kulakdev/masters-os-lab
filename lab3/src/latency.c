#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "../../shared/timer.h"

#define HOPS (2u * 1000u * 1000u)
#define WARMUP_HOPS (100u * 1000u)

static volatile uint32_t g_sink;

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

static int run_size(size_t bytes) {
    if (bytes < 2 * sizeof(uint32_t)) {
        return 0;
    }
    size_t n = bytes / sizeof(uint32_t);
    uint32_t *ring = xaligned_alloc(64, n * sizeof(uint32_t));
    uint32_t *order = malloc(n * sizeof(uint32_t));
    if (!order) {
        fprintf(stderr, "order allocation failed\n");
        free(ring);
        return 1;
    }

    for (size_t i = 0; i < n; i++) {
        order[i] = (uint32_t)i;
    }

    uint64_t state = 0x243F6A8885A308D3ull ^ (uint64_t)n;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rng_next(&state) % (i + 1));
        uint32_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    for (size_t i = 0; i < n; i++) {
        ring[order[i]] = order[(i + 1) % n];
    }
    free(order);

    uint32_t pos = 0;
    for (size_t i = 0; i < WARMUP_HOPS; i++) {
        pos = ring[pos];
    }

    double t0 = now_sec();
    for (size_t i = 0; i < HOPS; i++) {
        pos = ring[pos];
    }
    double ns = (now_sec() - t0) * 1e9 / (double)HOPS;
    g_sink ^= pos;

    char param[64];
    if (bytes >= 1024u * 1024u) {
        snprintf(param, sizeof(param), "workset_%zuMB", bytes / (1024u * 1024u));
    } else {
        snprintf(param, sizeof(param), "workset_%zuKB", bytes / 1024u);
    }
    emit("latency", param, ns, "ns_per_load");

    free(ring);
    return 0;
}

int bench_latency(void) {
    int rc = 0;
    for (size_t kb = 8; kb <= 128u * 1024u; kb *= 2) {
        rc |= run_size(kb * 1024u);
    }
    return rc;
}
