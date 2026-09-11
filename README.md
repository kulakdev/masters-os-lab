# OS Labs — Socket, IPC & Cache Benchmarking Suite

Three research-oriented labs in C11, each producing working demo programs and a comparison report.

| Lab | Topic | Status |
|-----|-------|--------|
| 1 | Socket performance (UNIX/INET, blocking/async, workloads) | In progress |
| 2 | IPC comparative analysis (mmap, shm, files, pipes, queues) | Planned |
| 3 | Cache behavior & memory optimization (access patterns, race conditions) | Planned |

## Repository Structure

```
.
├── CMakeLists.txt          # Root build: C11, platform detection, subdirectories
├── Dockerfile              # Debian bookworm-slim with gcc, cmake, python3
├── docker-compose.yml      # Containerized build + benchmark runner
├── Makefile                # One-command build/run shortcuts
├── shared/                 # Cross-lab utilities
│   ├── timer.h             # clock_gettime(CLOCK_MONOTONIC) wrapper
│   ├── ioadapter.h         # I/O multiplexer interface (epoll/kqueue/poll)
│   ├── iomux_epoll.c       # Linux backend
│   ├── iomux_kqueue.c      # macOS/BSD backend
│   └── iomux_poll.c        # Portable fallback
├── lab1/                   # Lab 1: Socket benchmarking
│   ├── CMakeLists.txt
│   ├── src/
│   ├── scripts/
│   └── report/
├── lab2/                   # Lab 2: IPC (planned)
├── lab3/                   # Lab 3: Cache (planned)
└── results/                # Benchmark output (CSV, generated reports)
```

## Prerequisites

### macOS (native build)
- Xcode Command Line Tools: `xcode-select --install`
- That's it — ships with AppleClang, `make`, and all POSIX headers needed.

### Linux (native build)
```bash
# Debian/Ubuntu
sudo apt install gcc cmake make libc6-dev python3

# Fedora
sudo dnf install gcc cmake make glibc-devel python3

# Arch
sudo pacman -S gcc cmake make python3
```

### Windows
- **Docker Desktop** — the only supported path on Windows.
  No native build (POSIX sockets and IPC APIs are unavailable outside WSL/Cygwin).
- WSL2 with Ubuntu is an alternative: follow the Linux instructions inside WSL.

### Docker (any platform)
- **Docker Desktop** (macOS, Windows) or **Docker Engine** (Linux)
- **Docker Compose v2** (bundled with Docker Desktop; on Linux: `sudo apt install docker-compose-plugin`)

## Building & Running

### Option A: Native (macOS or Linux)

```bash
make native
```

This runs:
1. `cmake -B build -DCMAKE_BUILD_TYPE=Release` — configures the build
2. `cmake --build build -j` — compiles all lab targets
3. `./build/lab1/socket_benchmark --all` — runs the lab1 binary

Platform detection happens automatically at configure time:
- **macOS:** CMake finds `kqueue` → compiles `iomux_kqueue.c`
- **Linux:** CMake finds `epoll` → compiles `iomux_epoll.c`

---

### Option B: Docker (recommended for reproducible benchmarks)

#### CLI
Build the container and drop into an interactive shell:
```bash
make docker
```

Inside the container, the project is mounted at `/app` with a fresh build:
```bash
# already built by the entrypoint, but to rebuild:
cmake --build build -j

# run lab1
./build-docker/lab1/socket_benchmark --all
```

#### Python Script
Run the full automated benchmark matrix (requires `lab1/scripts/run_benchmarks.py`):
```bash
make bench
```

Results land in `./results/` on your host filesystem.

---

### Option C: CLion IDE (macOS)

CLion can use your native macOS toolchain directly:
1. **Settings → Build → Toolchains** — verify AppleClang is detected
2. **Settings → Build → CMake** — set Build type to `Release`
3. Click the CMake reload button (or **Tools → CMake → Reload**)
4. Select `socket_benchmark` from the run configurations and hit Run/Debug

For Docker-based builds inside CLion:
1. **Settings → Build → Toolchains → + → Docker**
2. Point to the repo's Dockerfile
3. Move the Docker toolchain above AppleClang to make it default
4. Reload CMake — all builds now run inside the container

## Clean Build

```bash
make clean          # removes build/
rm -rf cmake-build-*  # removes stale CLion caches (if any)
```

## Platform Support Matrix

| Feature | macOS native | Linux native | Windows (Docker) |
|---------|:---:|:---:|:---:|
| Lab 1 — blocking sockets (AF_UNIX, AF_INET) | ✅ | ✅ | ✅ via Docker |
| Lab 1 — async I/O | kqueue | epoll | epoll (in container) |
| Lab 2 — mmap, pipes, FIFO | ✅ | ✅ | ✅ via Docker |
| Lab 2 — POSIX mqueue | ❌ | ✅ | ✅ via Docker |
| Lab 3 — cache benchmarks | ✅ | ✅ | ✅ via Docker |

## Benchmark Caveats

- **Docker on macOS/Windows** runs inside a Linux VM. Socket numbers reflect virtualized networking — relative comparisons (UNIX vs INET, blocking vs epoll) remain valid, but absolute throughput won't match bare-metal Linux. Document this in your report.
- **Apple Silicon (ARM)** containers run natively at near-native speed. x86 hosts emulate ARM via QEMU (slow) or run x86 containers natively. The benchmark CSV should record `uname -m` output.
- **Cache line size** differs: 128 bytes on Apple Silicon vs 64 bytes on x86. This matters for Lab 3 (false sharing, padding).
