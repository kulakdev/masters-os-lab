#ifdef __linux__

#include "ioadapter.h"
#include <sys/epoll.h>
#include <unistd.h>

int iomux_create(void) {
    return epoll_create1(0);
}

int iomux_add(int m, int fd, uint32_t events) {
    struct epoll_event ev = { .data.fd = fd, .events = 0 };
    if (events & IOMUX_READ)  ev.events |= EPOLLIN;
    if (events & IOMUX_WRITE) ev.events |= EPOLLOUT;
    return epoll_ctl(m, EPOLL_CTL_ADD, fd, &ev);
}

int iomux_wait(int m, io_event *events, int max, int timeout_ms) {
    struct epoll_event ev[256];
    if (max > 256) max = 256;

    int n = epoll_wait(m, ev, max, timeout_ms);
    for (int i = 0; i < n; i++) {
        events[i].fd = ev[i].data.fd;
        events[i].events = 0;
        if (ev[i].events & EPOLLIN)  events[i].events |= IOMUX_READ;
        if (ev[i].events & EPOLLOUT) events[i].events |= IOMUX_WRITE;
    }
    return n;
}

#endif /* __linux__ */
