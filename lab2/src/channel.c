#include "channel.h"
#include "timer.h"
#include <errno.h>
#include <fcntl.h>
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

static int run_throughput(const config_t *cfg, chan_t ch, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {                    /* consumer */
        for (long i = 0; i < cfg->msg_count; i++) {
            if (read_full(ch.rd, buf, cfg->msg_size) < 0) { free(buf); return -1; }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        if (write_full(ch.wr, buf, cfg->msg_size) < 0) { free(buf); return -1; }
    }
    int status = 0;
    waitpid(child_pid, &status, 0);
    double t1 = now_sec();

    res->duration_sec = t1 - t0;
    res->bytes = (uint64_t)cfg->msg_count * cfg->msg_size;
    res->msgs = cfg->msg_count;

    free(buf);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int run_latency(const config_t *cfg, chan_t ch, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {                    /* echo server */
        for (long i = 0; i < cfg->msg_count; i++) {
            if (read_full(ch.rd, buf, cfg->msg_size) < 0) { free(buf); return -1; }
            if (write_full(ch.wr, buf, cfg->msg_size) < 0) { free(buf); return -1; }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        if (write_full(ch.wr, buf, cfg->msg_size) < 0) { free(buf); return -1; }
        if (read_full(ch.rd, buf, cfg->msg_size) < 0) { free(buf); return -1; }
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

int chan_run(const config_t *cfg, chan_t ch, pid_t child_pid, results_t *res) {
    return cfg->test == T_LATENCY
        ? run_latency(cfg, ch, child_pid, res)
        : run_throughput(cfg, ch, child_pid, res);
}

uint64_t chan_capacity(int wr_fd) {
    int flags = fcntl(wr_fd, F_GETFL, 0);
    if (flags < 0) return 0;
    fcntl(wr_fd, F_SETFL, flags | O_NONBLOCK);

    static char chunk[65536];
    uint64_t total = 0;
    for (;;) {
        ssize_t n = write(wr_fd, chunk, sizeof chunk);
        if (n > 0) {
            total += (uint64_t)n;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    fcntl(wr_fd, F_SETFL, flags);
    return total;
}
