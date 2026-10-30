#include "common.h"
#include "timer.h"
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

#define FILE_A "/tmp/lab2_ipc_file_a.bin"
#define FILE_B "/tmp/lab2_ipc_file_b.bin"

static char *make_buffer(size_t size) {
    char *buf = malloc(size);
    if (buf) {
        for (size_t i = 0; i < size; i++) buf[i] = (char)(i & 0xff);
    }
    return buf;
}

static int pwrite_full(int fd, const void *buf, size_t len, off_t off) {
    const char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = pwrite(fd, p, left, off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        left -= (size_t)n;
        off += n;
    }
    return 0;
}

static int pread_full(int fd, void *buf, size_t len, off_t off) {
    char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = pread(fd, p, left, off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        left -= (size_t)n;
        off += n;
    }
    return 0;
}

/* File size is the only completion signal available across processes. */
static void wait_size(int fd, off_t need) {
    struct stat st;
    unsigned spin = 0;
    for (;;) {
        if (fstat(fd, &st) == 0 && st.st_size >= need) return;
        if ((++spin & 0x3ff) == 0) sched_yield();
    }
}

static int run_throughput(const config_t *cfg, int fd_a, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {
        for (long i = 0; i < cfg->msg_count; i++) {
            wait_size(fd_a, (off_t)((uint64_t)i + 1) * cfg->msg_size);
            if (pread_full(fd_a, buf, cfg->msg_size, (off_t)((uint64_t)i * cfg->msg_size)) < 0) {
                free(buf); return -1;
            }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        if (pwrite_full(fd_a, buf, cfg->msg_size, (off_t)((uint64_t)i * cfg->msg_size)) < 0) {
            free(buf); return -1;
        }
        if (cfg->fsync && fsync(fd_a) < 0) { free(buf); return -1; }
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

static int run_latency(const config_t *cfg, int fd_a, int fd_b, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {                   /* echo: A -> B */
        for (long i = 0; i < cfg->msg_count; i++) {
            off_t off = (off_t)((uint64_t)i * cfg->msg_size);
            wait_size(fd_a, off + (off_t)cfg->msg_size);
            if (pread_full(fd_a, buf, cfg->msg_size, off) < 0) { free(buf); return -1; }
            if (pwrite_full(fd_b, buf, cfg->msg_size, off) < 0) { free(buf); return -1; }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        off_t off = (off_t)((uint64_t)i * cfg->msg_size);
        if (pwrite_full(fd_a, buf, cfg->msg_size, off) < 0) { free(buf); return -1; }
        wait_size(fd_b, off + (off_t)cfg->msg_size);
        if (pread_full(fd_b, buf, cfg->msg_size, off) < 0) { free(buf); return -1; }
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

int bench_file(const config_t *cfg, results_t *res) {
    if (cfg->test == T_CAPACITY) {
        struct statvfs vfs;
        res->capacity_bytes = statvfs("/tmp", &vfs) == 0
            ? (uint64_t)vfs.f_bavail * vfs.f_frsize
            : 0;
        return 0;
    }

    unlink(FILE_A);
    unlink(FILE_B);
    int fd_a = open(FILE_A, O_CREAT | O_RDWR | O_TRUNC, 0600);
    int fd_b = open(FILE_B, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd_a < 0 || fd_b < 0) { perror("open"); return -1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    int rc;
    if (pid == 0) {
        rc = cfg->test == T_LATENCY
            ? run_latency(cfg, fd_a, fd_b, 0, res)
            : run_throughput(cfg, fd_a, 0, res);
        close(fd_a); close(fd_b);
        _exit(rc < 0 ? 1 : 0);
    }

    rc = cfg->test == T_LATENCY
        ? run_latency(cfg, fd_a, fd_b, pid, res)
        : run_throughput(cfg, fd_a, pid, res);

    close(fd_a); close(fd_b);
    unlink(FILE_A); unlink(FILE_B);
    return rc;
}
