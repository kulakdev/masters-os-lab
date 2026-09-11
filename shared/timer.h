//
// Created by macbook on 10.09.2026.
//

#pragma once
#include <time.h>

static inline double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}