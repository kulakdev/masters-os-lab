#include "channel.h"
#include "common.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int bench_socket(const config_t *cfg, results_t *res) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) { perror("socketpair"); return -1; }

    if (cfg->test == T_CAPACITY) {
        res->capacity_bytes = chan_capacity(sv[0]);
        close(sv[0]); close(sv[1]);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        close(sv[0]);
        chan_t ch = { .rd = sv[1], .wr = sv[1] };   /* duplex */
        int rc = chan_run(cfg, ch, 0, res);
        close(sv[1]);
        _exit(rc < 0 ? 1 : 0);
    }

    close(sv[1]);
    chan_t ch = { .rd = sv[0], .wr = sv[0] };
    int rc = chan_run(cfg, ch, pid, res);

    close(sv[0]);
    return rc;
}
