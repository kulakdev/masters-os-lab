#include "ring.h"
#include "timer.h"
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char *make_buffer(size_t size) {
    char *buf = malloc(size);
    if (buf) {
        for (size_t i = 0; i < size; i++) buf[i] = (char)(i & 0xff);
    }
    return buf;
}

static inline void spin_pause(unsigned *n) {
    if ((++*n & 0x3ff) == 0) sched_yield();
}

size_t ring_region_size(size_t msg_size) {
    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    size_t need = RING_CTL_PADDED + RING_SLOTS * msg_size;
    return (need + page - 1) / page * page;
}

int ring_init(ring_t *r, void *base, size_t region, size_t msg_size) {
    if (region < RING_CTL_PADDED || msg_size == 0) return -1;

    r->base = base;
    r->region = region;
    r->msg_size = msg_size;
    r->nslots = (region - RING_CTL_PADDED) / msg_size;
    if (r->nslots > RING_SLOTS) r->nslots = RING_SLOTS;
    if (r->nslots == 0) return -1;

    r->ctl = base;
    atomic_init(&r->ctl->head, 0);
    atomic_init(&r->ctl->tail, 0);
    atomic_init(&r->ctl->done, 0);
    atomic_init(&r->ctl->seq, 0);
    atomic_init(&r->ctl->ack, 0);

    r->slots = (char *)base + RING_CTL_PADDED;
    return 0;
}

uint64_t ring_capacity(const ring_t *r) {
    return (uint64_t)r->nslots * r->msg_size;
}

static int run_throughput(const config_t *cfg, ring_t *r, pid_t child_pid, results_t *res) {
    ring_ctl_t *ctl = r->ctl;
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {                    /* consumer */
        unsigned spin = 0;
        for (long i = 0; i < cfg->msg_count; i++) {
            while (atomic_load_explicit(&ctl->head, memory_order_acquire) <= (uint64_t)i)
                spin_pause(&spin);
            memcpy(buf, r->slots + ((size_t)i % r->nslots) * r->msg_size, r->msg_size);
            atomic_store_explicit(&ctl->tail, (uint64_t)i + 1, memory_order_release);
        }
        atomic_store_explicit(&ctl->done, (uint64_t)cfg->msg_count, memory_order_release);
        free(buf);
        return 0;
    }

    unsigned spin = 0;
    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        while ((uint64_t)i - atomic_load_explicit(&ctl->tail, memory_order_acquire) >= r->nslots)
            spin_pause(&spin);
        memcpy(r->slots + ((size_t)i % r->nslots) * r->msg_size, buf, r->msg_size);
        atomic_store_explicit(&ctl->head, (uint64_t)i + 1, memory_order_release);
    }
    while (atomic_load_explicit(&ctl->done, memory_order_acquire) < (uint64_t)cfg->msg_count)
        spin_pause(&spin);
    double t1 = now_sec();

    int status = 0;
    waitpid(child_pid, &status, 0);

    res->duration_sec = t1 - t0;
    res->bytes = (uint64_t)cfg->msg_count * cfg->msg_size;
    res->msgs = cfg->msg_count;

    free(buf);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int run_latency(const config_t *cfg, ring_t *r, pid_t child_pid, results_t *res) {
    ring_ctl_t *ctl = r->ctl;
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {                    /* echo */
        unsigned spin = 0;
        for (long i = 0; i < cfg->msg_count; i++) {
            while (atomic_load_explicit(&ctl->seq, memory_order_acquire) <= (uint64_t)i)
                spin_pause(&spin);
            memcpy(buf, r->slots, r->msg_size);
            atomic_store_explicit(&ctl->ack, (uint64_t)i + 1, memory_order_release);
        }
        free(buf);
        return 0;
    }

    unsigned spin = 0;
    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        memcpy(r->slots, buf, r->msg_size);
        atomic_store_explicit(&ctl->seq, (uint64_t)i + 1, memory_order_release);
        while (atomic_load_explicit(&ctl->ack, memory_order_acquire) <= (uint64_t)i)
            spin_pause(&spin);
    }
    double t1 = now_sec();

    int status = 0;
    waitpid(child_pid, &status, 0);

    res->duration_sec = t1 - t0;
    res->bytes = (uint64_t)cfg->msg_count * cfg->msg_size;
    res->msgs = cfg->msg_count;
    res->latency_us = (t1 - t0) / (double)cfg->msg_count * 0.5 * 1e6;

    free(buf);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

int ring_run(const config_t *cfg, ring_t *r, pid_t child_pid, results_t *res) {
    return cfg->test == T_LATENCY
        ? run_latency(cfg, r, child_pid, res)
        : run_throughput(cfg, r, child_pid, res);
}
