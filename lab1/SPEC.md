This is a spec for Agents to refer to when planning implementation.

---

# `SPEC.md` — Socket Performance Benchmarking Suite

## 1. Project Overview & Goal
The objective is to implement a research-oriented benchmarking suite written strictly in **pure C (C11/C17)** to measure and compare network socket performance across different domains, I/O paradigms, and workload configurations on Linux.

The output will produce reproducible empirical data (CSV/stdout) measuring **throughput (pkts/sec, MB/sec)**, **latency (connection setup/teardown)**, and generate a **comparison table with analytical conclusions** for coursework submission.

---

## 2. Hard Constraints
- **Language:** Pure C (strictly **C11** or **C17**). Non-C implementations incur a `-5` point penalty.
- **Operating Environment:** Linux POSIX environment (`glibc`). Must run containerized via Docker on macOS/Windows hosts.
- **Precision:** All timing must use `clock_gettime(CLOCK_MONOTONIC)` with sub-microsecond resolution.
- **Protocol Semantics:** Stream sockets (`SOCK_STREAM`) must handle message framing properly (handle partial reads/writes without data corruption or blocking deadlocks).

---

## 3. Dimensions Under Test

### 3.1 Socket Domains
1. **UNIX Domain Sockets (`AF_UNIX` / `SOCK_STREAM`):** Local IPC via filesystem paths or abstract namespace, bypassing network protocol stacks.
2. **Internet Sockets (`AF_INET` / `SOCK_STREAM`):** TCP over the local loopback interface (`127.0.0.1`), traversing the kernel TCP/IP stack.

### 3.2 I/O Modes & Concurrency Models
1. **Blocking (`SYNC_BLOCKING`):** Standard blocking `read()` / `write()` or `send()` / `recv()`.
2. **Non-blocking with Multiplexing (`ASYNC_EPOLL`):** Sockets set to `O_NONBLOCK` managed by Linux `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`).
3. *(Optional / Extension)* **Non-blocking Busy-poll or `poll()`:** For low-latency single-descriptor comparisons.

### 3.3 Workload Configurations
1. **Small Packet (Latency/Overhead Bound):**
    - Payload: `64 bytes`
    - Volume: `500,000 packets`
    - Focus: Syscall frequency, context switching, and header/IPC overhead.
2. **Medium Packet (Balanced):**
    - Payload: `4,096 bytes` (4 KB - standard memory page size)
    - Volume: `50,000 packets`
3. **Large Packet (Bandwidth Bound):**
    - Payload: `65,536 bytes` (64 KB)
    - Volume: `5,000 packets`
    - Focus: Memory copy (`memcpy`) speed, socket buffer saturation.

### 3.4 Target Metrics
- **Data Throughput:** Megabytes per second (`MB/s`).
- **Packet Throughput:** Packets per second (`pkts/s`).
- **Connection Setup Time:** Time to complete `connect()` + `accept()` in microseconds (`µs`).
- **Socket Teardown Time:** Time to `close()` and flush buffers in microseconds (`µs`).

---

## 4. System Architecture

```
                       +-----------------------------------+
                       |        Docker Container           |
                       |       (debian:bookworm-slim)      |
                       |                                   |
                       |   +---------------------------+   |
                       |   |    Benchmark Orchestrator |   |
                       |   |       (run_bench.sh)      |   |
                       |   +-------------+-------------+   |
                       |                 |                 |
                       |        Spawns per test run        |
                       |                 |                 |
                       |         +-------v-------+         |
                       |         |   fork()      |         |
                       |         +---+-------+---+         |
                       |             |       |             |
                       |   +---------v-+   +-v---------+   |
                       |   |  Server   |   |  Client   |   |
                       |   |  Process  |   |  Process  |   |
                       |   +-----+-----+   +-----+-----+   |
                       |         |               |         |
                       |         +-------+-------+         |
                       |                 |                 |
                       |    [AF_UNIX path or 127.0.0.1]    |
                       |     (SOCK_STREAM, Block/epoll)    |
                       +-----------------+-----------------+
```

To avoid inter-process race conditions during testing, the suite can be structured either as:
1. **A single binary (`socket_bench`)** capable of running as `--server`, `--client`, or `--all` (which internally `fork()`s the server and executes the benchmark synchronously).
2. **Two binaries (`bench_server`, `bench_client`)** coordinated via CLI flags.

*(Preferred for automated testing: Single binary with an `--all` harness mode, or separate client/server coordinated by a shell runner).*

---

## 5. CLI & Interface Design

The binary must accept configurable parameters via `getopt` or `getopt_long`:

```bash
socket_bench [OPTIONS]

Options:
  -d, --domain      unix | inet              (default: unix)
  -m, --mode        blocking | epoll         (default: blocking)
  -s, --size        <bytes>                  (packet payload size, default: 64)
  -n, --count       <num>                    (number of packets, default: 10000)
  -p, --port        <port>                   (TCP port for inet, default: 9876)
  -u, --unix-path   <path>                   (Socket file path, default: /tmp/bench.sock)
  -t, --test-conn                            (Flag: measure connection latency only)
  -r, --role        server | client | all    (default: all)
  --csv                                      (Output machine-readable CSV row)
  -h, --help                                 (Display help message)
```

### 5.1 Output Format
When `--csv` is passed, output a single row without headers (orchestrator handles headers):
```csv
domain,io_mode,msg_size_bytes,msg_count,duration_sec,throughput_mb_s,packets_per_sec,conn_setup_us,conn_teardown_us
```

When run interactively (default), print human-readable summary stats:
```text
======================================================
Benchmark Run: AF_INET | ASYNC_EPOLL
Payload Size:  4096 bytes | Packets: 50,000
======================================================
Connection Setup Time: 124.32 us
Elapsed Time:          0.412 s
Packet Rate:           121,359 pkts/sec
Throughput:            474.06 MB/sec
Teardown Time:         45.10 us
======================================================
```

---

## 6. Directory Structure

```text
.
├── CMakeLists.txt              # CMake build definitions (C11, flags: -O3, -Wall, -Wextra)
├── Dockerfile                  # Container environment (Debian/Ubuntu, gcc, cmake, python3)
├── SPEC.md                     # This specification
├── README.md                   # Setup, build, run instructions
├── scripts/
│   ├── run_benchmarks.py       # Automated runner iterating through the matrix -> outputs results.csv
│   └── generate_report.py      # Parses results.csv -> generates Markdown table & plots (optional)
├── src/
│   ├── main.c                  # CLI parsing and dispatch
│   ├── common.h                # Shared structs, constants, framing definitions, timer utils
│   ├── common.c                # High-res timer implementations, socket helper functions
│   ├── server.c                # Server logic (blocking & epoll)
│   ├── server.h
│   ├── client.c                # Client logic (packet generator & collector)
│   └── client.h
└── report/
    └── REPORT.md               # Final coursework mini-report with tables and conclusions
```

---

## 7. Execution Matrix

The automated script (`scripts/run_benchmarks.py` or shell equivalent) will execute a minimum of 12 distinct configurations (running each 3–5 times and reporting the median):

| Run # | Domain | I/O Mode | Payload Size | Packet Count |
|:---:|:---:|:---:|:---:|:---:|
| 1 | `AF_UNIX` | Blocking | 64 B | 500,000 |
| 2 | `AF_UNIX` | Blocking | 4,096 B | 50,000 |
| 3 | `AF_UNIX` | Blocking | 65,536 B | 5,000 |
| 4 | `AF_UNIX` | Non-blocking (`epoll`) | 64 B | 500,000 |
| 5 | `AF_UNIX` | Non-blocking (`epoll`) | 4,096 B | 50,000 |
| 6 | `AF_UNIX` | Non-blocking (`epoll`) | 65,536 B | 5,000 |
| 7 | `AF_INET` | Blocking | 64 B | 500,000 |
| 8 | `AF_INET` | Blocking | 4,096 B | 50,000 |
| 9 | `AF_INET` | Blocking | 65,536 B | 5,000 |
| 10 | `AF_INET` | Non-blocking (`epoll`) | 64 B | 500,000 |
| 11 | `AF_INET` | Non-blocking (`epoll`) | 4,096 B | 50,000 |
| 12 | `AF_INET` | Non-blocking (`epoll`) | 65,536 B | 5,000 |

*Connection Setup / Teardown Tests:*
- 1,000 sequential `connect()` + `close()` cycles for `AF_UNIX` vs. `AF_INET`.

---

## 8. Requirements for the Mini-Report (`REPORT.md`)
The generated report must contain:
1. **Test Environment Details:** Host CPU, OS, Docker version, GCC version, compiler flags (`-O3`).
2. **Comparison Table:** Full aggregation of results (Throughput, Packet Rate, Latency).
3. **In-depth Analysis & Conclusions (in Ukrainian or English):**
    - *AF_UNIX vs AF_INET:* Explanation of performance gap (IPC skipping IP checksums, routing, TCP state machine, segment assembly).
    - *Blocking vs Epoll (Async):* Explanation of why blocking can sometimes be faster for single-client point-to-point streaming (absence of `epoll_ctl`/`epoll_wait` syscall overhead), but epoll shines in multiplexed concurrency.
    - *Packet Size Impact:* Explanation of CPU-bound behavior at small sizes (syscall overhead dominates) vs memory/bus-bound behavior at large sizes (`memcpy` and kernel socket buffer boundaries).
    - *Connection Setup Cost:* Comparison between filesystem inode lookup/bind vs 3-way TCP handshake over loopback.