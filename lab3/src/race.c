#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "bench.h"
#include "../../shared/timer.h"

#define THREADS 4
#define ITERS 500000u

typedef struct {
    int idx;
    void *shared;
    pthread_mutex_t *mutex;
    uint64_t local_result;
} worker_arg;

static volatile long g_volatile_counter;
static _Atomic long g_atomic_relaxed;
static _Atomic long g_atomic_seqcst;
static long g_mutex_counter;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *worker_volatile(void *p) {
    (void)p;
    for (size_t i = 0; i < ITERS; i++) {
        g_volatile_counter = g_volatile_counter + 1;
    }
    return NULL;
}

static void *worker_atomic_relaxed(void *p) {
    (void)p;
    for (size_t i = 0; i < ITERS; i++) {
        atomic_fetch_add_explicit(&g_atomic_relaxed, 1, memory_order_relaxed);
    }
    return NULL;
}

static void *worker_atomic_seqcst(void *p) {
    (void)p;
    for (size_t i = 0; i < ITERS; i++) {
        atomic_fetch_add(&g_atomic_seqcst, 1);
    }
    return NULL;
}

static void *worker_mutex(void *p) {
    (void)p;
    for (size_t i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&g_mutex);
        g_mutex_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    return NULL;
}

static void *worker_local(void *p) {
    worker_arg *a = p;
    uint64_t local = 0;
    for (size_t i = 0; i < ITERS; i++) {
        local++;
    }
    a->local_result = local;
    return NULL;
}

static double run(void *(*worker)(void *), const char *label) {
    pthread_t tids[THREADS];
    worker_arg args[THREADS];

    double t0 = now_sec();
    for (int i = 0; i < THREADS; i++) {
        args[i].idx = i;
        args[i].local_result = 0;
        if (pthread_create(&tids[i], NULL, worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            exit(1);
        }
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(tids[i], NULL);
    }
    double secs = now_sec() - t0;

    emit("race", label, secs * 1e3, "ms");
    return secs;
}

static void report_value(const char *label, long value, long expected) {
    char param[96];
    snprintf(param, sizeof(param), "%s_final", label);
    emit("race", param, (double)value, "final_value");
    snprintf(param, sizeof(param), "%s_correct", label);
    emit("race", param, value == expected ? 1.0 : 0.0, "bool");
}

int bench_race(void) {
    const long expected = (long)THREADS * (long)ITERS;
    char param[64];
    snprintf(param, sizeof(param), "expected");
    emit("race", param, (double)expected, "final_value");

    g_volatile_counter = 0;
    g_atomic_relaxed = 0;
    g_atomic_seqcst = 0;
    g_mutex_counter = 0;

    run(worker_volatile, "volatile_no_sync");
    report_value("volatile_no_sync", g_volatile_counter, expected);

    run(worker_atomic_relaxed, "atomic_relaxed");
    report_value("atomic_relaxed", atomic_load_explicit(&g_atomic_relaxed, memory_order_relaxed),
                 expected);

    run(worker_atomic_seqcst, "atomic_seqcst");
    report_value("atomic_seqcst", atomic_load(&g_atomic_seqcst), expected);

    run(worker_mutex, "mutex");
    report_value("mutex", g_mutex_counter, expected);

    {
        pthread_t tids[THREADS];
        worker_arg args[THREADS];
        double t0 = now_sec();
        for (int i = 0; i < THREADS; i++) {
            args[i].idx = i;
            args[i].local_result = 0;
            pthread_create(&tids[i], NULL, worker_local, &args[i]);
        }
        long total = 0;
        for (int i = 0; i < THREADS; i++) {
            pthread_join(tids[i], NULL);
            total += (long)args[i].local_result;
        }
        double secs = now_sec() - t0;
        emit("race", "local_then_reduce", secs * 1e3, "ms");
        report_value("local_then_reduce", total, expected);
    }

    return 0;
}
