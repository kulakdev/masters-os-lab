#include "common.h"
#include "ring.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_NAME "/lab2_ipc_shm"

int bench_shm(const config_t *cfg, results_t *res) {
    size_t region = ring_region_size(cfg->msg_size);

    if (cfg->test == T_CAPACITY) {
        size_t nslots = (region - RING_CTL_PADDED) / cfg->msg_size;
        if (nslots > RING_SLOTS) nslots = RING_SLOTS;
        res->capacity_bytes = (uint64_t)nslots * cfg->msg_size;
        return 0;
    }

    shm_unlink(SHM_NAME);                       /* drop stale segment */
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return -1; }
    if (ftruncate(fd, (off_t)region) < 0) { perror("ftruncate"); close(fd); shm_unlink(SHM_NAME); return -1; }

    void *base = mmap(NULL, region, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED) { perror("mmap"); shm_unlink(SHM_NAME); return -1; }

    ring_t ring;
    if (ring_init(&ring, base, region, cfg->msg_size) < 0) {
        munmap(base, region);
        shm_unlink(SHM_NAME);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); munmap(base, region); shm_unlink(SHM_NAME); return -1; }

    if (pid == 0) {
        int rc = ring_run(cfg, &ring, 0, res);
        munmap(base, region);
        _exit(rc < 0 ? 1 : 0);
    }

    int rc = ring_run(cfg, &ring, pid, res);
    munmap(base, region);
    shm_unlink(SHM_NAME);
    return rc;
}
