#pragma once
#include <stdint.h>

#define IOMUX_READ  (1u << 0)
#define IOMUX_WRITE (1u << 1)

typedef struct {
    int      fd;
    uint32_t events;
} io_event;

int  iomux_create(void);
int  iomux_add(int m, int fd, uint32_t events);
int  iomux_wait(int m, io_event *events, int max, int timeout_ms);