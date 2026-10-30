#include "channel.h"
#include "common.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int bench_pipe(const config_t *cfg, results_t *res) {
    int ab[2], ba[2];
    if (pipe(ab) < 0 || pipe(ba) < 0) { perror("pipe"); return -1; }

    if (cfg->test == T_CAPACITY) {
        res->capacity_bytes = chan_capacity(ab[1]);
        close(ab[0]); close(ab[1]); close(ba[0]); close(ba[1]);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        close(ab[1]);               /* child reads ab, writes ba */
        close(ba[0]);
        chan_t ch = { .rd = ab[0], .wr = ba[1] };
        int rc = chan_run(cfg, ch, 0, res);
        _exit(rc < 0 ? 1 : 0);
    }

    close(ab[0]);                   /* parent writes ab, reads ba */
    close(ba[1]);
    chan_t ch = { .rd = ba[0], .wr = ab[1] };
    int rc = chan_run(cfg, ch, pid, res);

    close(ab[1]); close(ba[0]);
    return rc;
}
