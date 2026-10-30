#include "common.h"
#include "ring.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MMAP_FILE "/tmp/lab2_ipc_mmap.bin"

int bench_mmap(const config_t *cfg, results_t *res) {
    size_t region = ring_region_size(cfg->msg_size);
    int file_backed = (cfg->method == M_MMAP_FILE);

    if (cfg->test == T_CAPACITY) {
        size_t nslots = (region - RING_CTL_PADDED) / cfg->msg_size;
        if (nslots > RING_SLOTS) nslots = RING_SLOTS;
        res->capacity_bytes = (uint64_t)nslots * cfg->msg_size;
        return 0;
    }

    void *base;
    int fd = -1;

    if (file_backed) {
        unlink(MMAP_FILE);
        fd = open(MMAP_FILE, O_CREAT | O_RDWR | O_TRUNC, 0600);
        if (fd < 0) { perror("open mmap file"); return -1; }
        if (ftruncate(fd, (off_t)region) < 0) { perror("ftruncate"); close(fd); return -1; }
        base = mmap(NULL, region, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
    } else {
        base = mmap(NULL, region, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    }

    if (base == MAP_FAILED) { perror("mmap"); return -1; }

    ring_t ring;
    if (ring_init(&ring, base, region, cfg->msg_size) < 0) {
        munmap(base, region);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); munmap(base, region); return -1; }

    if (pid == 0) {
        int rc = ring_run(cfg, &ring, 0, res);
        munmap(base, region);
        _exit(rc < 0 ? 1 : 0);
    }

    int rc = ring_run(cfg, &ring, pid, res);
    munmap(base, region);
    if (file_backed) unlink(MMAP_FILE);
    return rc;
}
