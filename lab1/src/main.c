#include "common.h"
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n"
           "  -d, --domain     unix | inet          (default: unix)\n"
           "  -m, --mode       blocking | epoll     (default: blocking)\n"
           "  -s, --size       <bytes>              (default: 64)\n"
           "  -n, --count      <num>                (default: 10000)\n"
           "  -p, --port       <port>               (default: 9876)\n"
           "  -u, --unix-path  <path>               (default: /tmp/bench.sock)\n"
           "  -t, --test-conn                       measure conn latency only\n"
           "  -r, --role       server | client | all (default: all)\n"
           "      --csv                             machine-readable output\n"
           "  -h, --help\n", prog);
}

static void print_results(const config_t *cfg, const results_t *r) {
    const char *dom = cfg->domain == DOMAIN_UNIX ? "unix" : "inet";
    const char *mod = cfg->mode == MODE_BLOCKING ? "blocking" : "epoll";

    double mbps  = r->duration_sec > 0 ? (double)r->bytes / r->duration_sec / 1e6 : 0;
    double pps   = r->duration_sec > 0 ? (double)r->packets / r->duration_sec : 0;

    if (cfg->csv) {
        printf("%s,%s,%zu,%ld,%.6f,%.2f,%.0f,%.2f,%.2f\n",
               dom, mod, cfg->msg_size, cfg->msg_count,
               r->duration_sec, mbps, pps, r->conn_setup_us, r->teardown_us);
    } else {
        printf("======================================================\n");
        printf("Benchmark Run: %s | %s\n",
               cfg->domain == DOMAIN_UNIX ? "AF_UNIX" : "AF_INET",
               cfg->mode == MODE_BLOCKING ? "SYNC_BLOCKING" : "ASYNC_EPOLL");
        printf("Payload Size:  %zu bytes | Packets: %ld\n", cfg->msg_size, cfg->msg_count);
        printf("======================================================\n");
        printf("Connection Setup Time: %.2f us\n", r->conn_setup_us);
        printf("Elapsed Time:          %.3f s\n",  r->duration_sec);
        printf("Packet Rate:           %.0f pkts/sec\n", pps);
        printf("Throughput:            %.2f MB/sec\n", mbps);
        printf("Teardown Time:         %.2f us\n", r->teardown_us);
        printf("======================================================\n");
    }
}

int main(int argc, char **argv) {
    config_t cfg = {
        .domain = DOMAIN_UNIX, .mode = MODE_BLOCKING, .role = ROLE_ALL,
        .msg_size = 64, .msg_count = 10000, .port = 9876,
        .unix_path = "/tmp/bench.sock", .test_conn = 0, .csv = 0,
    };

    static struct option long_opts[] = {
        {"domain",    required_argument, 0, 'd'},
        {"mode",      required_argument, 0, 'm'},
        {"size",      required_argument, 0, 's'},
        {"count",     required_argument, 0, 'n'},
        {"port",      required_argument, 0, 'p'},
        {"unix-path", required_argument, 0, 'u'},
        {"test-conn", no_argument,       0, 't'},
        {"role",      required_argument, 0, 'r'},
        {"csv",       no_argument,       0, 'c'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int o;
    while ((o = getopt_long(argc, argv, "d:m:s:n:p:u:tr:ch", long_opts, NULL)) != -1) {
        switch (o) {
            case 'd': cfg.domain = strcmp(optarg, "inet") == 0 ? DOMAIN_INET : DOMAIN_UNIX; break;
            case 'm': cfg.mode   = strcmp(optarg, "epoll") == 0 ? MODE_EPOLL : MODE_BLOCKING; break;
            case 's': cfg.msg_size  = (size_t)atol(optarg); break;
            case 'n': cfg.msg_count = atol(optarg);         break;
            case 'p': cfg.port      = atoi(optarg);         break;
            case 'u': cfg.unix_path = optarg;               break;
            case 't': cfg.test_conn = 1;                    break;
            case 'r':
                if (strcmp(optarg, "server") == 0)      cfg.role = ROLE_SERVER;
                else if (strcmp(optarg, "client") == 0) cfg.role = ROLE_CLIENT;
                break;
            case 'c': cfg.csv = 1; break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    signal(SIGPIPE, SIG_IGN);   /* dying on SIGPIPE would corrupt benchmarks */

    if (cfg.role == ROLE_SERVER) return run_server(&cfg) < 0 ? 1 : 0;

    results_t res;
    if (cfg.role == ROLE_CLIENT) {
        if (run_client(&cfg, &res) < 0) return 1;
        print_results(&cfg, &res);
        return 0;
    }

    /* ROLE_ALL: create listener BEFORE fork => no readiness race */
    int lfd = make_listen_socket(&cfg);
    if (lfd < 0) return 1;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {             /* child: server */
        int rc = serve_connection(&cfg, lfd);
        close(lfd);
        _exit(rc < 0 ? 1 : 0);  /* _exit: skip stdio flush in child */
    }

    /* parent: client (listener already exists, connect succeeds immediately) */
    int rc = run_client(&cfg, &res);
    int status = 0;
    waitpid(pid, &status, 0);
    close(lfd);
    if (cfg.domain == DOMAIN_UNIX) unlink(cfg.unix_path);

    if (rc < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "benchmark failed (client rc=%d, server status=%d)\n", rc, status);
        return 1;
    }
    print_results(&cfg, &res);
    return 0;
}