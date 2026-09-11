#if !defined(__linux__) && !defined(__APPLE__) && !defined(__FreeBSD__)

#include "ioadapter.h"
#include <poll.h>

#define IOMUX_MAX_FDS 1024

static struct pollfd g_fds[IOMUX_MAX_FDS];
static int g_nfds;

int iomux_create(void) {
    g_nfds = 0;
    return 0;
}

int iomux_add(int m, int fd, uint32_t events) {
    (void)m;
    if (g_nfds >= IOMUX_MAX_FDS) return -1;
    g_fds[g_nfds].fd = fd;
    g_fds[g_nfds].events = 0;
    if (events & IOMUX_READ)  g_fds[g_nfds].events |= POLLIN;
    if (events & IOMUX_WRITE) g_fds[g_nfds].events |= POLLOUT;
    g_nfds++;
    return 0;
}

int iomux_wait(int m, io_event *events, int max, int timeout_ms) {
    (void)m;
    int n = poll(g_fds, (nfds_t)g_nfds, timeout_ms);
    if (n <= 0) return n;

    int out = 0;
    for (int i = 0; i < g_nfds && out < max; i++) {
        if (!g_fds[i].revents) continue;
        events[out].fd = g_fds[i].fd;
        events[out].events = 0;
        if (g_fds[i].revents & POLLIN)  events[out].events |= IOMUX_READ;
        if (g_fds[i].revents & POLLOUT) events[out].events |= IOMUX_WRITE;
        out++;
    }
    return out;
}

#endif /* fallback */
