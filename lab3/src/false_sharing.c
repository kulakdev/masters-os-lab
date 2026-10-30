#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "bench.h"
#include "../../shared/timer.h"

#define THREADS 4
#define ITERS (20u * 1000u * 1000u)

static volatile uint64_t g_sink;

typedef struct {
    void *counter;
    int idx;
} worker_arg;

typedef struct {
    volatile uint64_t v;
    char pad[128 - sizeof(uint64_t)];
} padded_counter;

static void *worker_volatile(void *p) {
    worker_arg *a = p;
    volatile uint64_t *cells = a->counter;
    for (size_t i = 0; i < ITERS; i++) {
        cells[a->idx] = cells[a->idx] + 1;
    }
    return NULL;
}

static void *worker_padded_volatile(void *p) {
    worker_arg *a = p;
    padded_counter *cells = a->counter;
    for (size_t i = 0; i < ITERS; i++) {
        cells[a->idx].v = cells[a->idx].v + 1;
    }
    return NULL;
}

static void *worker_atomic(void *p) {
    worker_arg *a = p;
    _Atomic uint64_t *cells = a->counter;
    for (size_t i = 0; i < ITERS; i++) {
        atomic_fetch_add_explicit(&cells[a->idx], 1, memory_order_relaxed);
    }
    return NULL;
}

static double run(void *(*worker)(void *), void *counters, const char *label) {
    pthread_t tids[THREADS];
    worker_arg args[THREADS];

    double t0 = now_sec();
    for (int i = 0; i < THREADS; i++) {
        args[i].counter = counters;
        args[i].idx = i;
        if (pthread_create(&tids[i], NULL, worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            exit(1);
        }
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(tids[i], NULL);
    }
    double secs = now_sec() - t0;

    double total_increments = (double)THREADS * (double)ITERS;
    emit("false-sharing", label, secs * 1e3, "ms");
    emit("false-sharing", label, secs * 1e9 / total_increments, "ns_per_increment");
    return secs;
}

int bench_false_sharing(void) {
    {
        volatile uint64_t *packed = xaligned_alloc(128, THREADS * sizeof(uint64_t));
        run(worker_volatile, (void *)packed, "packed_volatile");
        for (int i = 0; i < THREADS; i++) {
            g_sink ^= packed[i];
        }
        free((void *)packed);
    }

    {
        padded_counter *padded = xaligned_alloc(128, THREADS * sizeof(padded_counter));
        run(worker_padded_volatile, padded, "padded_volatile");
        for (int i = 0; i < THREADS; i++) {
            g_sink ^= padded[i].v;
        }
        free(padded);
    }

    {
        _Atomic uint64_t *atomic_packed = xaligned_alloc(128, THREADS * sizeof(_Atomic uint64_t));
        run(worker_atomic, atomic_packed, "packed_atomic_relaxed");
        for (int i = 0; i < THREADS; i++) {
            g_sink ^= atomic_load_explicit(&atomic_packed[i], memory_order_relaxed);
        }
        free(atomic_packed);
    }

    return 0;
}
