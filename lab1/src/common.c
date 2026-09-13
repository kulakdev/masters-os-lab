//
// Created by macbook on 13.09.2026.
//

#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Function helper to bootstrap an address based on the domain
static int fill_addr(const config_t *cfg, struct sockaddr_storage *ss, socklen_t *len) {
    // define a range the length of *ss and fill with zeroes for now
    memset(ss, 0, sizeof *ss);
    if (cfg->domain == DOMAIN_UNIX) {
        struct sockaddr_un *un = (struct sockaddr_un *)ss;
        un->sun_family = AF_UNIX;
        strncpy(un->sun_path, cfg->unix_path, sizeof(un->sun_path) - 1);
        // set the out-parameter
        *len = sizeof(*un);
        return AF_UNIX;
    }
    struct sockaddr_in *in = (struct sockaddr_in *)ss;
    in->sin_family = AF_INET;
    in->sin_port   = htons((uint64_t)cfg->port);
    in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    // set the out-parameter
    *len = sizeof(*in);
    return AF_INET;
}

// Util for setting up a listening socket
int make_listen_socket(const config_t *cfg) {
    struct sockaddr_storage ss; socklen_t len;
    int family = fill_addr(cfg, &ss, &len);

    int fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1;}

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    if (cfg->domain == DOMAIN_UNIX) unlink(cfg->unix_path); /* stale socket file */

    if (bind(fd, (struct sockaddr *)&ss, len) < 0) { perror("bind"); return -1;}
    if (listen(fd, 16) < 0)                      { perror("listen"); return -1;}
    return fd;
}

int make_connect_socket(const config_t *cfg) {
    struct sockaddr_storage ss; socklen_t len;
    int family = fill_addr(cfg, &ss, &len);

    int fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    if (cfg->domain == DOMAIN_INET) {
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    }

    if (connect(fd, (struct sockaddr *)&ss, len) < 0) { perror("connect"); close(fd); return -1; }
    return fd;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t sent = 0;
    while ( sent < len ) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

ssize_t recv_all(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, p+got, len-got, 0);
        if (n < 0) {
            if (errno = EINTR) continue;
            return -1;
        }
        if (n == 0) break; /*EOF: peer closed early*/
        got += (size_t)n;
    }
    return (ssize_t)got;
}