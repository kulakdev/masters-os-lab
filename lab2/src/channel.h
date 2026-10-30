#pragma once
#include "common.h"
#include <sys/types.h>

/*
 * Bidirectional fd channel: `rd` is read, `wr` is write.
 * For duplex descriptors (socketpair) both fields hold the same fd.
 */
typedef struct {
    int rd;
    int wr;
} chan_t;

/*
 * Run one side of the benchmark in the current process.
 * child_pid == 0  -> this process is the consumer (child)
 * child_pid  > 0  -> this process is the producer (parent), waits for the child
 * Return 0 on success. Parent fills `res`; child leaves it untouched.
 */
int chan_run(const config_t *cfg, chan_t ch, pid_t child_pid, results_t *res);

/* Fill the write end in non-blocking mode until EAGAIN; returns buffered bytes. */
uint64_t chan_capacity(int wr_fd);
