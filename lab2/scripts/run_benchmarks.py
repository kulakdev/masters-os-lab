#!/usr/bin/env python3
"""Run the full lab2 IPC benchmark matrix and collect results into results/lab2_results.csv."""

import csv
import os
import platform
import statistics
import subprocess
import sys
import time

BINARY_CANDIDATES = [
    "build/lab2/ipc_benchmark",
    "build-docker/lab2/ipc_benchmark",
    "../build/lab2/ipc_benchmark",
    "../build-docker/lab2/ipc_benchmark",
]

METHODS = ["mmap_file", "mmap_anon", "shm", "file", "pipe", "fifo", "socket", "mqueue"]
SIZES = [64, 4096, 65536]
THROUGHPUT_COUNTS = {64: 200_000, 4096: 50_000, 65536: 5_000}
LATENCY_COUNT = 50_000
CAPACITY_SIZE = 4096
FSYNC_SIZE = 64
FSYNC_COUNT = 2_000
RUNS_PER_CONFIG = 3

CSV_FIELDS = [
    "method", "test", "msg_size_bytes", "msg_count",
    "duration_sec", "throughput_mb_s", "msgs_per_sec",
    "latency_us", "capacity_bytes",
]
NUMERIC_FIELDS = [
    "duration_sec", "throughput_mb_s", "msgs_per_sec", "latency_us", "capacity_bytes",
]


def find_binary():
    env = os.environ.get("LAB2_BINARY")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return os.path.abspath(env)
    for path in BINARY_CANDIDATES:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return os.path.abspath(path)
    return None


def build():
    build_dir = "build-docker" if os.path.exists("/.dockerenv") else "build"
    subprocess.run(["cmake", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", build_dir, "-j"], check=True)


def run_once(binary, method, test, size, count, fsync=False):
    cmd = [binary, "-m", method, "-t", test, "-s", str(size), "-n", str(count), "--csv"]
    if fsync:
        cmd.append("--fsync")
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        return None
    line = result.stdout.strip().split("\n")[-1]
    return dict(zip(CSV_FIELDS, line.split(",")))


def median_run(binary, method, test, size, count, fsync=False):
    runs = []
    for _ in range(RUNS_PER_CONFIG):
        r = run_once(binary, method, test, size, count, fsync)
        if r:
            runs.append(r)
        time.sleep(0.05)
    if not runs:
        return None
    med = dict(runs[0])
    for k in NUMERIC_FIELDS:
        vals = [float(r[k]) for r in runs]
        med[k] = f"{statistics.median(vals):.6f}"
    return med


def collect_env():
    lines = [f"platform: {platform.platform()}",
             f"machine: {platform.machine()}",
             f"processor: {platform.processor()}"]
    for cmd in (["gcc", "--version"], ["clang", "--version"], ["cmake", "--version"]):
        try:
            out = subprocess.run(cmd, capture_output=True, text=True).stdout
            if out:
                lines.append(f"{cmd[0]}: {out.splitlines()[0]}")
        except Exception:
            pass
    if sys.platform == "darwin":
        try:
            cpu = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"],
                                 capture_output=True, text=True).stdout.strip()
            lines.append(f"cpu: {cpu}")
        except Exception:
            pass
    if os.path.exists("/.dockerenv"):
        lines.append("runtime: docker")
    else:
        lines.append("runtime: native")
    return "\n".join(lines)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    os.chdir(project_root)

    binary = find_binary()
    if not binary:
        print("Binary not found, building...")
        build()
        binary = find_binary()
    if not binary:
        print("ERROR: could not find or build ipc_benchmark", file=sys.stderr)
        sys.exit(1)

    print(f"Using binary: {binary}")
    print(f"Platform: {platform.platform()} ({platform.machine()})")
    print(f"Runs per config: {RUNS_PER_CONFIG}\n")

    results_dir = os.path.join(os.getcwd(), "results")
    os.makedirs(results_dir, exist_ok=True)
    csv_path = os.path.join(results_dir, "lab2_results.csv")
    env_path = os.path.join(results_dir, "lab2_env.txt")

    with open(env_path, "w") as f:
        f.write(collect_env())

    jobs = []
    for size in SIZES:
        for method in METHODS:
            jobs.append((method, "throughput", size, THROUGHPUT_COUNTS[size], False))
    for method in METHODS:
        jobs.append((method, "latency", 64, LATENCY_COUNT, False))
    for method in METHODS:
        jobs.append((method, "capacity", CAPACITY_SIZE, 1, False))
    jobs.append(("file_fsync", "throughput", FSYNC_SIZE, FSYNC_COUNT, True))

    all_rows = []
    skipped = []

    for i, (method, test, size, count, fsync) in enumerate(jobs, 1):
        label = f"{method:10s} | {test:10s} | {size:>6d}B | n={count:>7d}"
        print(f"[{i:>2}/{len(jobs)}] {label} ... ", end="", flush=True)

        call_method = "file" if method == "file_fsync" else method
        row = median_run(binary, call_method, test, size, count, fsync)
        if row is None:
            skipped.append((method, test, size))
            print("SKIPPED (not supported / failed)")
            continue

        row["method"] = method
        all_rows.append(row)
        if test == "throughput":
            print(f"{float(row['throughput_mb_s']):>10.2f} MB/s  {float(row['msgs_per_sec']):>12.0f} msgs/s")
        elif test == "latency":
            print(f"{float(row['latency_us']):>10.3f} us one-way")
        else:
            print(f"{float(row['capacity_bytes']):>14.0f} bytes")

    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(all_rows)

    print(f"\nResults written to: {csv_path}")
    print(f"Environment info:  {env_path}")
    if skipped:
        print("Skipped configurations:")
        for m, t, s in skipped:
            print(f"  - {m} | {t} | {s}B")


if __name__ == "__main__":
    main()
