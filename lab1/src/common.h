#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { DOMAIN_UNIX, DOMAIN_INET }           domain_t;
typedef enum { MODE_BLOCKING, MODE_EPOLL }          io_mode_t;
typedef enum { ROLE_SERVER, ROLE_CLIENT, ROLE_ALL } role_t;

typedef struct {
    domain_t   domain;
    io_mode_t  mode;
    role_t     role;
    size_t     msg_size;
    long       msg_count;
    int        port;
    const char *unix_path;
    int        test_conn;
    int        csv;
} config_t;

typedef struct {
    double   conn_setup_us;
    double   teardown_us;
    double   duration_sec;
    uint64_t bytes;
    long     packets;
} results_t;

// commmon.c
int     make_listen_socket(const config_t *cfg);
int     make_connect_socket(const config_t *cfg);
int     set_nonblocking(int fd);
ssize_t send_all(int fd, const void *buf, size_t len);
ssize_t recv_all(int fd, void *buf, size_t len);


// server.c
int run_server(const config_t *cfg);                /* standalone */
int serve_connection(const config_t *cfg, int lfd); /* accept from an existing listener */

// client.c
int run_client(const config_t *cfg, results_t *out);