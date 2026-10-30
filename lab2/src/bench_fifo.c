#include "channel.h"
#include "common.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_A "/tmp/lab2_ipc_fifo_a"
#define FIFO_B "/tmp/lab2_ipc_fifo_b"

int bench_fifo(const config_t *cfg, results_t *res) {
    unlink(FIFO_A);
    unlink(FIFO_B);
    if (mkfifo(FIFO_A, 0600) < 0 || mkfifo(FIFO_B, 0600) < 0) {
        perror("mkfifo");
        return -1;
    }

    if (cfg->test == T_CAPACITY) {
        /* Open both ends in O_RDWR so the call never blocks, then fill. */
        int fd = open(FIFO_A, O_RDWR | O_NONBLOCK);
        if (fd < 0) { perror("open fifo"); return -1; }
        res->capacity_bytes = chan_capacity(fd);
        close(fd);
        unlink(FIFO_A); unlink(FIFO_B);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        int rd = open(FIFO_A, O_RDONLY);    /* pairs with parent's writer */
        int wr = open(FIFO_B, O_WRONLY);
        if (rd < 0 || wr < 0) { perror("open fifo"); _exit(1); }
        chan_t ch = { .rd = rd, .wr = wr };
        int rc = chan_run(cfg, ch, 0, res);
        _exit(rc < 0 ? 1 : 0);
    }

    int wr = open(FIFO_A, O_WRONLY);        /* pairs with child's reader */
    int rd = open(FIFO_B, O_RDONLY);
    if (wr < 0 || rd < 0) { perror("open fifo"); return -1; }

    chan_t ch = { .rd = rd, .wr = wr };
    int rc = chan_run(cfg, ch, pid, res);

    close(rd); close(wr);
    unlink(FIFO_A); unlink(FIFO_B);
    return rc;
}
