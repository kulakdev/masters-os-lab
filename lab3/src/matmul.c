#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "../../shared/timer.h"

#define BLOCK 64

static volatile double g_sink;

static void fill(double *m, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            m[i * n + j] = (double)((i * 7 + j * 3) % 17) * 0.01 + 1.0;
        }
    }
}

static void clear(double *m, size_t n) {
    for (size_t i = 0; i < n * n; i++) {
        m[i] = 0.0;
    }
}

static double checksum(const double *m, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n * n; i += 64) {
        s += m[i];
    }
    return s;
}

static double matmul_ijk(const double *a, const double *b, double *c, size_t n) {
    double t0 = now_sec();
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            double sum = 0.0;
            for (size_t k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
        }
    }
    return now_sec() - t0;
}

static double matmul_ikj(const double *a, const double *b, double *c, size_t n) {
    double t0 = now_sec();
    for (size_t i = 0; i < n; i++) {
        for (size_t k = 0; k < n; k++) {
            double av = a[i * n + k];
            for (size_t j = 0; j < n; j++) {
                c[i * n + j] += av * b[k * n + j];
            }
        }
    }
    return now_sec() - t0;
}

static double matmul_blocked(const double *a, const double *b, double *c, size_t n) {
    double t0 = now_sec();
    for (size_t ii = 0; ii < n; ii += BLOCK) {
        for (size_t kk = 0; kk < n; kk += BLOCK) {
            for (size_t jj = 0; jj < n; jj += BLOCK) {
                size_t i_max = ii + BLOCK < n ? ii + BLOCK : n;
                size_t k_max = kk + BLOCK < n ? kk + BLOCK : n;
                size_t j_max = jj + BLOCK < n ? jj + BLOCK : n;
                for (size_t i = ii; i < i_max; i++) {
                    for (size_t k = kk; k < k_max; k++) {
                        double av = a[i * n + k];
                        for (size_t j = jj; j < j_max; j++) {
                            c[i * n + j] += av * b[k * n + j];
                        }
                    }
                }
            }
        }
    }
    return now_sec() - t0;
}

static int run_size(size_t n) {
    double *a = xaligned_alloc(64, n * n * sizeof(double));
    double *b = xaligned_alloc(64, n * n * sizeof(double));
    double *c = xaligned_alloc(64, n * n * sizeof(double));
    fill(a, n);
    fill(b, n);

    const double flops = 2.0 * (double)n * (double)n * (double)n;

    struct {
        const char *name;
        double (*fn)(const double *, const double *, double *, size_t);
    } variants[] = {
        {"ijk", matmul_ijk},
        {"ikj", matmul_ikj},
        {"blocked64", matmul_blocked},
    };

    for (size_t v = 0; v < sizeof(variants) / sizeof(variants[0]); v++) {
        clear(c, n);
        double secs = variants[v].fn(a, b, c, n);
        g_sink += checksum(c, n);

        char param[64];
        snprintf(param, sizeof(param), "%s_N%zu", variants[v].name, n);
        emit("matmul", param, secs * 1e3, "ms");
        emit("matmul", param, flops / (secs * 1e9), "GFLOP_per_s");
    }

    free(a);
    free(b);
    free(c);
    return 0;
}

int bench_matmul(void) {
    int rc = 0;
    rc |= run_size(512);
    rc |= run_size(1024);
    return rc;
}
