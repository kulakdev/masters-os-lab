//
// Created by macbook on 13.09.2026.
//
#include "common.h"
#include "timer.h"
#include "ioadapter.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static int client_blocking(const config_t *cfg, int fd, char *buf) {
    for (long i = 0; i < cfg->msg_count; i++) {
        if (send_all(fd, buf, cfg->msg_size) < 0) { perror("send"); return -1; }
    }
    return 0;
}

static int client_epoll(const config_t *cfg, int fd, char *buf) {
    if (set_nonblocking(fd) < 0) { perror("fcntl"); return -1; }
    int mux = iomux_create();
    if (mux < 0 || iomux_add(mux, fd, IOMUX_WRITE) < 0) { perror("iomux"); return -1;}

    uint64_t total = (uint64_t)cfg->msg_size * (uint64_t)cfg->msg_count;
    uint64_t sent = 0;
    io_event ev;
    while (sent < total) {
        size_t chunk = cfg->msg_size;
        if (total - sent < chunk) chunk = (size_t)(total - sent);
        ssize_t n = send(fd, buf, chunk, 0);
        if (n > 0) { sent += (uint64_t)n; continue; }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (iomux_wait(mux, &ev, 1, -1) < 0 && errno != EINTR) {
                perror("iomux_wait"); close(mux); return -1;
            }
            continue;
        }
        perror("send"); close(mux); return -1;
    }
    close(mux);
    return 0;
}

int run_client(const config_t *cfg, results_t *out) {
    memset(out, 0, sizeof *out);

    if (cfg->test_conn) {
        for (long i = 0; i < cfg->msg_count; i++) {
            double t0 = now_sec();
            int fd = make_connect_socket(cfg);
            double t1= now_sec();
            if (fd < 0) return -1;
            double c0 = now_sec();
            close(fd);
            double c1 = now_sec();
            out->conn_setup_us += (t1 - t0) * 1e6;
            out->teardown_us   += (c1 - c0) * 1e6;
        }
        out->conn_setup_us /= (double)cfg->msg_count;
        out->teardown_us    /= (double)cfg->msg_count;
        return 0;
    }

    double t0 = now_sec();
    int fd = make_connect_socket(cfg);
    double t1 = now_sec();
    if (fd < 0) return -1;
    out->conn_setup_us = (t1 - t0) * 1e6;

    char *buf = malloc(cfg->msg_size);
    if (!buf) { close(fd); return -1; }
    memset(buf, 0xAB, cfg->msg_size);   /* garbage noise payload */

    double s0 = now_sec();
    int rc = (cfg->mode == MODE_EPOLL) ? client_epoll(cfg, fd, buf)
                                       : client_blocking(cfg, fd, buf);
    double s1 = now_sec();

    shutdown(fd, SHUT_WR);              /* clean EOF signal for the server */

    double c0 = now_sec();
    close(fd);
    double c1 = now_sec();

    out->teardown_us  = (c1 - c0) * 1e6;
    out->duration_sec = s1 - s0;
    out->bytes        = (uint64_t)cfg->msg_size * (uint64_t)cfg->msg_count;
    out->packets      = cfg->msg_count;
    free(buf);
    return rc;
}