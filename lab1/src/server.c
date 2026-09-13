//
// Created by macbook on 13.09.2026.
//
#include "common.h"
#include "ioadapter.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

static int server_blocking(const config_t *cfg, int cfd) {
    uint64_t total = (uint64_t)cfg->msg_size * (uint64_t)cfg->msg_count;
    char *buf = malloc(cfg->msg_size);
    if (!buf) return -1;

    uint64_t got = 0;
    while (got < total) {
        ssize_t n = recv(cfd, buf, cfg->msg_size, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv"); free(buf); return -1;
        }
        if (n == 0) break;
        got += (uint64_t)n;
    }
    // Benchmark so we never consume the noise that arrives
    free(buf);
    return 0;
}

static int server_epoll(const config_t *cfg, int cfd) {
    if (set_nonblocking(cfd) < 0) { perror("fnctl"); return -1; }
    int mux = iomux_create();
    if (mux < 0 || iomux_add(mux, cfd, IOMUX_READ) < 0) { perror("iomux"); return -1; }

    uint64_t total = (uint64_t)cfg->msg_size * (uint64_t)cfg->msg_count;
    char *buf = malloc(cfg->msg_size);
    if (!buf) return -1;

    uint64_t got = 0;
    io_event ev;
    while (got < total) {
        ssize_t n = recv(cfd, buf, cfg->msg_size, 0);
        if (n > 0) { got += (uint64_t)n; continue; }
        if (n == 0) break;
        if (errno == EINTR) continue;
        // Async is literally an error that returns status EAGAIN bruh...
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // sleep if not arrived yet.
            if (iomux_wait(mux, &ev, 1, -1) < 0 && errno != EINTR) {
                perror("iomux_wait"); free(buf); close(mux); return -1;
            }
            continue;
        }
        perror("recv"); free(buf); close(mux); return -1;
    }
    free(buf);
    close(mux);
    return 0;
}

int serve_connection(const config_t *cfg, int lfd) {
    if (cfg->test_conn) {
        for (long i=0; i < cfg -> msg_count; i++) {
            int cfd = accept(lfd, NULL, NULL);
            if (cfd < 0) { if (errno == EINTR) { i--; continue; } perror("accept"); return -1; }
            close(cfd);
        }
        return 0;
    }

    int cfd = accept(lfd, NULL, NULL);
    if (cfd < 0 ) { perror("accept"); return -1; }
    int rc = (cfg->mode == MODE_EPOLL) ? server_epoll(cfg, cfd)
                                       : server_blocking(cfg, cfd);
    close(cfd);
    return rc;
}

int run_server(const config_t *cfg) {
    int lfd = make_listen_socket(cfg);
    if (lfd < 0) return -1;
    int rc = serve_connection(cfg, lfd);
    close(lfd);
    if (cfg->domain == DOMAIN_UNIX) unlink(cfg->unix_path);
    return rc;
}