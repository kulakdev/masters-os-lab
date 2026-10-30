This is a spec for Agents to refer to when planning implementation.

---

# `SPEC.md` — Inter-Process Communication (IPC) Comparative Benchmarking Suite

## 1. Project Overview & Goal

The objective is to implement a research-oriented benchmarking suite written strictly in **pure C (C11)** to measure and compare different IPC mechanisms on POSIX systems.

The suite produces reproducible empirical data (CSV/stdout) measuring **latency (µs)**, **throughput (MB/s, msgs/s)**, and **channel capacity (bytes)**, and generates a **comparison table with analytical conclusions** for coursework submission.

---

## 2. Hard Constraints

- **Language:** Pure C (strictly **C11**). Non-C implementations incur a `-5` point penalty.
- **Operating Environment:** POSIX. Linux is the reference platform (runs containerized via Docker on macOS hosts). POSIX message queues are Linux-only and must be compile-time guarded.
- **Precision:** All timing must use `clock_gettime(CLOCK_MONOTONIC)` (see `shared/timer.h`).
- **Synchronization in shared memory:** C11 `<stdatomic.h>` lock-free atomics (work across `fork()` in `MAP_SHARED` mappings on both Linux and macOS).

---

## 3. IPC Methods Under Test

| # | Method | CLI name | Mechanism |
|---|--------|----------|-----------|
| 1 | mmap, file-backed | `mmap_file` | `open()` + `ftruncate()` + `mmap(MAP_SHARED)` on a regular file |
| 2 | mmap, anonymous | `mmap_anon` | `mmap(MAP_SHARED\|MAP_ANONYMOUS)` (parent/child after `fork()`) |
| 3 | POSIX shared memory | `shm` | `shm_open()` + `ftruncate()` + `mmap(MAP_SHARED)` |
| 4 | Regular file I/O | `file` | `open()`/`write()`/`pread()` with `fstat()` size polling; optional `--fsync` |
| 5 | Anonymous pipe | `pipe` | `pipe()` |
| 6 | Named pipe (FIFO) | `fifo` | `mkfifo()` + `open()` |
| 7 | UNIX domain socket | `socket` | `socketpair(AF_UNIX, SOCK_STREAM)` |
| 8 | POSIX message queue | `mqueue` | `mq_open()`/`mq_send()`/`mq_receive()` — **Linux only** |

Shared-memory methods (1–3) use a bounded **ring buffer** (64 slots) with cache-line-padded atomic head/tail counters; latency uses a seq/ack ping-pong protocol over the same mapping.

---

## 4. Test Modes & Metrics

### 4.1 Throughput (`throughput`)
Producer streams `msg_count` messages of `msg_size` bytes to the consumer; duration is measured from the first send until the consumer has received **all** messages (end-to-end). Metrics: `MB/s`, `msgs/s`.

### 4.2 Latency (`latency`)
Ping-pong: producer sends a message and waits for the consumer's echo, `msg_count` round trips. Metric: one-way latency = RTT/2 in µs.

### 4.3 Channel Capacity (`capacity`)
- `pipe` / `fifo` / `socket`: set write end `O_NONBLOCK`, fill until `EAGAIN`, count bytes.
- `mqueue`: `mq_send(...O_NONBLOCK)` until `EAGAIN` → `mq_maxmsg × mq_msgsize`.
- `mmap_*` / `shm`: ring payload capacity (`nslots × msg_size`).
- `file`: free space on the backing filesystem (`fstatvfs`) — effectively unbounded.

---

## 5. CLI & Interface Design

```bash
ipc_benchmark [OPTIONS]

Options:
  -m, --method   mmap_file | mmap_anon | shm | file | pipe | fifo | socket | mqueue
  -t, --test     throughput | latency | capacity        (default: throughput)
  -s, --size     <bytes>                                (default: 64)
  -n, --count    <num>                                  (default: 10000)
      --fsync                                           fsync() after each write (file method)
      --csv                                             machine-readable output
  -h, --help
```

### 5.1 CSV Output Format

```csv
method,test,msg_size_bytes,msg_count,duration_sec,throughput_mb_s,msgs_per_sec,latency_us,capacity_bytes
```

Single row, no header (orchestrator handles headers).

---

## 6. Architecture

```
                +------------------------------------------+
                |        ipc_benchmark (single binary)     |
                |                                          |
                |   parent (producer)  <==IPC==>  child (consumer)
                |        fork()-based harness              |
                +------------------------------------------+
```

Each bench module (`bench_*.c`) is self-contained: it creates the IPC channel, `fork()`s, runs producer (parent, measures time, `waitpid()`) and consumer (child, `_exit()`), cleans up (unlink/munmap).

- `channel.c` — generic fd-based runners (pipe/fifo/socket share read/write logic)
- `ring.c` — shared-memory ring + ping-pong runners (mmap_file/mmap_anon/shm)
- `bench_file.c`, `bench_mqueue.c` — custom implementations

---

## 7. Directory Structure

```text
lab2/
├── SPEC.md
├── CMakeLists.txt
├── src/
│   ├── main.c              # CLI parsing and dispatch
│   ├── common.h/.c         # config/results types, method tables, read_full/write_full
│   ├── channel.h/.c        # fd-channel throughput/latency/capacity runners
│   ├── ring.h/.c           # shared-memory ring buffer runners
│   ├── bench_pipe.c
│   ├── bench_fifo.c
│   ├── bench_socket.c
│   ├── bench_file.c
│   ├── bench_mmap.c        # both mmap modes (file-backed and anonymous)
│   ├── bench_shm.c
│   └── bench_mqueue.c      # Linux only (#ifdef HAVE_MQUEUE)
├── scripts/
│   ├── run_benchmarks.py   # matrix runner -> results/lab2_results.csv
│   └── plot_results.py     # CSV -> bar charts (matplotlib)
└── report/
    └── REPORT.md           # mini-report with tables and conclusions
```

---

## 8. Execution Matrix

Methods × sizes {64 B, 4 KB, 64 KB} × tests {throughput, latency} + capacity per method. Each configuration runs 3–5 times, median is reported.

- mqueue: message size clamped to 8 KB kernel limit (`msgsize_max`); 64 KB skipped.
- Latency runs use 64 B payloads.
- `file` method additionally runs once with `--fsync` for comparison.

---

## 9. Requirements for the Mini-Report (`REPORT.md`)

1. **Test Environment Details:** host CPU, OS, compiler, flags, Docker vs native.
2. **Comparison Tables:** latency, throughput (per size), capacity for every method.
3. **Analysis & Conclusions (Ukrainian):**
   - Why shared memory is fastest (no syscalls in steady state, zero copy).
   - mmap modes: file vs anonymous vs shm_open — setup cost differences.
   - Pipe vs FIFO (same kernel mechanism, namespace difference).
   - Socket vs pipe (full duplex, socket buffers).
   - File I/O (page cache vs `fsync`, no native notification mechanism).
   - mqueue (kernel-copied messages, fixed small capacity, priority support).
   - Capacity vs buffering trade-offs, when to choose which mechanism.
