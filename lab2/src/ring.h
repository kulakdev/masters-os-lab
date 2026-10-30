#pragma once
#include "common.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define RING_SLOTS 64

/*
 * Control block at the base of the shared mapping. Every counter sits on its
 * own cache line (128 bytes covers both x86-64 and Apple Silicon) so producer
 * and consumer do not bounce the same line between cores (false sharing).
 */
typedef struct {
    _Alignas(128) _Atomic uint64_t head;
    _Alignas(128) _Atomic uint64_t tail;
    _Alignas(128) _Atomic uint64_t done;
    _Alignas(128) _Atomic uint64_t seq;
    _Alignas(128) _Atomic uint64_t ack;
} ring_ctl_t;

#define RING_CTL_PADDED sizeof(ring_ctl_t)  /* 5 counters x 128-byte lines */

typedef struct {
    void    *base;
    size_t   region;
    size_t   msg_size;
    size_t   nslots;
    ring_ctl_t *ctl;
    char    *slots;
} ring_t;

/* Size of the mapping to request for `msg_size` (header + RING_SLOTS slots). */
size_t ring_region_size(size_t msg_size);

/* Initialise the ring over an existing shared mapping (call before fork()). */
int  ring_init(ring_t *r, void *base, size_t region, size_t msg_size);

/* Payload capacity of the ring in bytes. */
uint64_t ring_capacity(const ring_t *r);

/*
 * Run one side of a shared-memory benchmark.
 * child_pid == 0 -> consumer, child_pid > 0 -> producer (waits for child).
 */
int ring_run(const config_t *cfg, ring_t *r, pid_t child_pid, results_t *res);
