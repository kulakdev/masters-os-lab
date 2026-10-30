#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include "bench.h"

uint64_t cache_line_size(void) {
#if defined(__APPLE__)
    uint64_t line = 0;
    size_t len = sizeof(line);
    if (sysctlbyname("hw.cachelinesize", &line, &len, NULL, 0) == 0 && line > 0) {
        return line;
    }
    return 128;
#elif defined(_SC_LEVEL1_DCACHE_LINESIZE)
    long line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    return line > 0 ? (uint64_t)line : 64;
#else
    return 64;
#endif
}

void emit(const char *bench, const char *param, double value, const char *unit) {
    printf("%s,%s,%.4f,%s\n", bench, param, value, unit);
    fflush(stdout);
}

void *xaligned_alloc(size_t alignment, size_t size) {
    size_t rounded = (size + alignment - 1) / alignment * alignment;
    void *p = NULL;
    if (posix_memalign(&p, alignment, rounded) != 0) {
        fprintf(stderr, "allocation of %zu bytes failed\n", rounded);
        exit(1);
    }
    memset(p, 0, rounded);
    return p;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
            "usage: %s <benchmark>\n"
            "  mem            sequential vs random access over a large array\n"
            "  stride         stride sweep (cache line / prefetcher behaviour)\n"
            "  latency        working-set latency curve (L1/L2/L3/RAM)\n"
            "  matmul         matrix multiplication: ijk vs ikj vs blocked\n"
            "  false-sharing  packed vs padded per-thread counters\n"
            "  race           racy increment vs atomic vs mutex\n"
            "  all            run everything (CSV on stdout)\n",
            prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    fprintf(stderr, "# cache_benchmark: %llu-byte cache line, %ld logical CPUs\n",
            (unsigned long long)cache_line_size(), sysconf(_SC_NPROCESSORS_ONLN));
    printf("benchmark,parameter,value,unit\n");

    const char *what = argv[1];
    int rc = 0;

    if (strcmp(what, "mem") == 0) {
        rc = bench_mem_pattern();
    } else if (strcmp(what, "stride") == 0) {
        rc = bench_stride();
    } else if (strcmp(what, "latency") == 0) {
        rc = bench_latency();
    } else if (strcmp(what, "matmul") == 0) {
        rc = bench_matmul();
    } else if (strcmp(what, "false-sharing") == 0) {
        rc = bench_false_sharing();
    } else if (strcmp(what, "race") == 0) {
        rc = bench_race();
    } else if (strcmp(what, "all") == 0) {
        rc |= bench_mem_pattern();
        rc |= bench_stride();
        rc |= bench_latency();
        rc |= bench_matmul();
        rc |= bench_false_sharing();
        rc |= bench_race();
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return rc;
}
