#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifdef HAVE_MQUEUE

#include "timer.h"
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sched.h>
#include <unistd.h>

#define MQ_A "/lab2_ipc_mq_a"
#define MQ_B "/lab2_ipc_mq_b"
#define MQ_MAXMSG 10
#define MQ_MSGSIZE_MAX 8192      /* Linux default /proc/sys/fs/mqueue/msgsize_max */

static char *make_buffer(size_t size) {
    char *buf = malloc(size);
    if (buf) {
        for (size_t i = 0; i < size; i++) buf[i] = (char)(i & 0xff);
    }
    return buf;
}

static mqd_t mq_create(const char *name, size_t msg_size, int flags) {
    struct mq_attr attr;
    memset(&attr, 0, sizeof attr);
    attr.mq_maxmsg = MQ_MAXMSG;
    attr.mq_msgsize = (long)msg_size;
    return mq_open(name, flags | O_CREAT, 0600, &attr);
}

static int run_throughput(const config_t *cfg, mqd_t out, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {
        for (long i = 0; i < cfg->msg_count; i++) {
            if (mq_receive(out, buf, cfg->msg_size, NULL) < 0) { free(buf); return -1; }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        if (mq_send(out, buf, cfg->msg_size, 1) < 0) { free(buf); return -1; }
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

static int run_latency(const config_t *cfg, mqd_t out, mqd_t in, pid_t child_pid, results_t *res) {
    char *buf = make_buffer(cfg->msg_size);
    if (!buf) return -1;

    if (child_pid == 0) {
        for (long i = 0; i < cfg->msg_count; i++) {
            if (mq_receive(in, buf, cfg->msg_size, NULL) < 0) { free(buf); return -1; }
            if (mq_send(out, buf, cfg->msg_size, 1) < 0) { free(buf); return -1; }
        }
        free(buf);
        return 0;
    }

    double t0 = now_sec();
    for (long i = 0; i < cfg->msg_count; i++) {
        if (mq_send(out, buf, cfg->msg_size, 1) < 0) { free(buf); return -1; }
        if (mq_receive(in, buf, cfg->msg_size, NULL) < 0) { free(buf); return -1; }
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

int bench_mqueue(const config_t *cfg, results_t *res) {
    if (cfg->msg_size > MQ_MSGSIZE_MAX) {
        fprintf(stderr, "mqueue: msg size %zu exceeds kernel limit %d\n",
                cfg->msg_size, MQ_MSGSIZE_MAX);
        return -1;
    }
    if (cfg->test == T_CAPACITY && cfg->msg_size == 0) return -1;

    mq_unlink(MQ_A);
    mq_unlink(MQ_B);

    if (cfg->test == T_CAPACITY) {
        mqd_t q = mq_create(MQ_A, cfg->msg_size, O_RDWR | O_NONBLOCK);
        if (q == (mqd_t)-1) { perror("mq_open"); return -1; }
        char *buf = make_buffer(cfg->msg_size);
        if (!buf) { mq_close(q); mq_unlink(MQ_A); return -1; }
        uint64_t total = 0;
        for (;;) {
            if (mq_send(q, buf, cfg->msg_size, 1) < 0) break;
            total += cfg->msg_size;
        }
        if (errno != EAGAIN) { free(buf); mq_close(q); mq_unlink(MQ_A); return -1; }
        res->capacity_bytes = total;
        free(buf);
        mq_close(q);
        mq_unlink(MQ_A);
        return 0;
    }

    if (cfg->test == T_LATENCY) {
        mqd_t qa = mq_create(MQ_A, cfg->msg_size, O_RDWR);
        mqd_t qb = mq_create(MQ_B, cfg->msg_size, O_RDWR);
        if (qa == (mqd_t)-1 || qb == (mqd_t)-1) { perror("mq_open"); return -1; }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }
        if (pid == 0) {
            int rc = run_latency(cfg, qb, qa, 0, res);   /* echo A -> B */
            mq_close(qa); mq_close(qb);
            _exit(rc < 0 ? 1 : 0);
        }
        int rc = run_latency(cfg, qa, qb, pid, res);
        mq_close(qa); mq_close(qb);
        mq_unlink(MQ_A); mq_unlink(MQ_B);
        return rc;
    }

    mqd_t q = mq_create(MQ_A, cfg->msg_size, O_RDWR);
    if (q == (mqd_t)-1) { perror("mq_open"); return -1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        int rc = run_throughput(cfg, q, 0, res);
        mq_close(q);
        _exit(rc < 0 ? 1 : 0);
    }
    int rc = run_throughput(cfg, q, pid, res);
    mq_close(q);
    mq_unlink(MQ_A);
    return rc;
}

#else  /* !HAVE_MQUEUE */

int bench_mqueue(const config_t *cfg, results_t *res) {
    (void)cfg; (void)res;
    fprintf(stderr, "mqueue: POSIX message queues are not available on this platform\n");
    return -1;
}

#endif
