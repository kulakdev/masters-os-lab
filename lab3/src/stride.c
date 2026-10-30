#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "../../shared/timer.h"

#define BUFFER_BYTES (32u * 1024u * 1024u)
#define ACCESSES (4u * 1000u * 1000u)

static volatile uint64_t g_sink;

int bench_stride(void) {
    const size_t n = BUFFER_BYTES / sizeof(uint64_t);
    uint64_t *buf = xaligned_alloc(64, n * sizeof(uint64_t));

    for (size_t i = 0; i < n; i++) {
        buf[i] = i;
    }

    static const size_t strides[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

    for (size_t s = 0; s < sizeof(strides) / sizeof(strides[0]); s++) {
        size_t stride = strides[s];
        uint64_t acc = 0;
        double t0 = now_sec();
        for (size_t k = 0; k < ACCESSES; k++) {
            acc += buf[(k * stride) & (n - 1)];
        }
        double ns = (now_sec() - t0) * 1e9 / (double)ACCESSES;
        g_sink ^= acc;

        char param[64];
        snprintf(param, sizeof(param), "stride_%zuB", stride * sizeof(uint64_t));
        emit("stride", param, ns, "ns_per_access");
    }

    free(buf);
    return 0;
}
