//
// Created by macbook on 10.09.2026.
//

/* kqueue backend — compiled only on macOS/BSD via ioadapter.c */
#include <stdint.h>
#include <sys/event.h>
#include <unistd.h>

#include "ioadapter.h"

int iomux_create(void) {
    return kqueue();
}

int iomux_add(int m, int fd, uint32_t events) {
    struct kevent kev[2];
    int n = 0;
    if (events & IOMUX_READ)
        EV_SET(&kev[n++], fd, EVFILT_READ,  EV_ADD, 0, 0, NULL);
    if (events & IOMUX_WRITE)
        EV_SET(&kev[n++], fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
    return kevent(m, kev, n, NULL, 0, NULL);
}

int iomux_wait(int m, io_event *events, int max, int timeout_ms) {
    struct kevent kev[256];
    if (max > 256) max = 256;

    struct timespec ts = { timeout_ms / 1000, (timeout_ms % 1000) * 1000000L };
    int n = kevent(m, NULL, 0, kev, max, timeout_ms >= 0 ? &ts : NULL);

    for (int i = 0; i < n; i++) {
        events[i].fd = (int)kev[i].ident;
        events[i].events = 0;
        if (kev[i].filter == EVFILT_READ)  events[i].events |= IOMUX_READ;
        if (kev[i].filter == EVFILT_WRITE) events[i].events |= IOMUX_WRITE;
    }
    return n;
}