#include "common.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*bench_fn)(const config_t *cfg, results_t *res);

static bench_fn const benches[M_COUNT] = {
    [M_MMAP_FILE] = bench_mmap,
    [M_MMAP_ANON] = bench_mmap,
    [M_SHM]       = bench_shm,
    [M_FILE]      = bench_file,
    [M_PIPE]      = bench_pipe,
    [M_FIFO]      = bench_fifo,
    [M_SOCKET]    = bench_socket,
    [M_MQUEUE]    = bench_mqueue,
};

static void usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n"
           "  -m, --method  mmap_file | mmap_anon | shm | file | pipe | fifo | socket | mqueue\n"
           "  -t, --test    throughput | latency | capacity      (default: throughput)\n"
           "  -s, --size    <bytes>                              (default: 64)\n"
           "  -n, --count   <num>                                (default: 10000)\n"
           "      --fsync                                        fsync each write (file method)\n"
           "      --csv                                          machine-readable output\n"
           "  -h, --help\n", prog);
}

static void print_results(const config_t *cfg, const results_t *r) {
    double mbps = r->duration_sec > 0 ? (double)r->bytes / r->duration_sec / 1e6 : 0;
    double mps  = r->duration_sec > 0 ? (double)r->msgs / r->duration_sec : 0;

    if (cfg->csv) {
        printf("%s,%s,%zu,%ld,%.6f,%.2f,%.0f,%.2f,%llu\n",
               method_name(cfg->method), test_name(cfg->test),
               cfg->msg_size, cfg->msg_count, r->duration_sec, mbps, mps,
               r->latency_us, (unsigned long long)r->capacity_bytes);
        return;
    }

    printf("======================================================\n");
    printf("IPC Benchmark: %s | %s\n", method_name(cfg->method), test_name(cfg->test));
    printf("Message Size:  %zu bytes | Messages: %ld%s\n",
           cfg->msg_size, cfg->msg_count, cfg->fsync ? " | fsync" : "");
    printf("======================================================\n");

    if (cfg->test == T_CAPACITY) {
        printf("Channel Capacity:      %llu bytes (%.1f KiB)\n",
               (unsigned long long)r->capacity_bytes, (double)r->capacity_bytes / 1024.0);
    } else {
        printf("Elapsed Time:          %.6f s\n", r->duration_sec);
        if (cfg->test == T_LATENCY) {
            printf("One-way Latency:       %.3f us\n", r->latency_us);
        } else {
            printf("Message Rate:          %.0f msgs/sec\n", mps);
            printf("Throughput:            %.2f MB/sec\n", mbps);
        }
    }
    printf("======================================================\n");
}

int main(int argc, char **argv) {
    config_t cfg = {
        .method = M_PIPE, .test = T_THROUGHPUT,
        .msg_size = 64, .msg_count = 10000, .fsync = 0, .csv = 0,
    };

    static struct option long_opts[] = {
        {"method", required_argument, 0, 'm'},
        {"test",   required_argument, 0, 't'},
        {"size",   required_argument, 0, 's'},
        {"count",  required_argument, 0, 'n'},
        {"fsync",  no_argument,       0, 'f'},
        {"csv",    no_argument,       0, 'c'},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int o;
    while ((o = getopt_long(argc, argv, "m:t:s:n:fch", long_opts, NULL)) != -1) {
        switch (o) {
            case 'm':
                if (parse_method(optarg, &cfg.method) < 0) {
                    fprintf(stderr, "unknown method: %s\n", optarg);
                    usage(argv[0]);
                    return 1;
                }
                break;
            case 't':
                if (parse_test(optarg, &cfg.test) < 0) {
                    fprintf(stderr, "unknown test: %s\n", optarg);
                    usage(argv[0]);
                    return 1;
                }
                break;
            case 's': cfg.msg_size  = (size_t)atol(optarg); break;
            case 'n': cfg.msg_count = atol(optarg);         break;
            case 'f': cfg.fsync = 1; break;
            case 'c': cfg.csv = 1;   break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    if (cfg.msg_size == 0 || cfg.msg_count <= 0) {
        fprintf(stderr, "size and count must be positive\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);   /* a dead peer must not kill the benchmark */

    results_t res;
    memset(&res, 0, sizeof res);

    if (benches[cfg.method](&cfg, &res) < 0) {
        fprintf(stderr, "benchmark failed: %s | %s\n",
                method_name(cfg.method), test_name(cfg.test));
        return 1;
    }

    print_results(&cfg, &res);
    return 0;
}
