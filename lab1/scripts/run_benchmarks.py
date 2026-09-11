#!/usr/bin/env python3
"""Run the full lab1 benchmark matrix and collect results into results/results.csv."""

import csv
import os
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

BINARY_CANDIDATES = [
    "build/lab1/socket_benchmark",
    "build-docker/lab1/socket_benchmark",
    "../build/lab1/socket_benchmark",
    "../build-docker/lab1/socket_benchmark",
]

MATRIX = [
    # (domain, io_mode, payload_size, packet_cont)
    ("unix", "blocking", 64,     500_000),
    ("unix", "blocking", 4096,    50_000),
    ("unix", "blocking", 65536,    5_000),
    ("unix", "epoll",    64,     500_000),
    ("unix", "epoll",    4096,    50_000),
    ("unix", "epoll",    65536,    5_000),
    ("inet", "blocking", 64,     500_000),
    ("inet", "blocking", 4096,    50_000),
    ("inet", "blocking", 65536,    5_000),
    ("inet", "epoll",    64,     500_000),
    ("inet", "epoll",    4096,    50_000),
    ("inet", "epoll",    65536,    5_000),
]

CONN_TEST_CYCLES = 1000
RUNS_PER_CONFIG = 3
CSV_FIELDS = [
    "domain", "io_mode", "msg_size_bytes", "msg_count",
    "duration_sec", "throughput_mb_s", "packets_per_sec",
    "conn_setup_us", "conn_teardown_us",
]


def find_binary():
    for path in BINARY_CANDIDATES:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return os.path.abspath(path)
    return None


def build():
    build_dir = "build-docker" if os.path.exists("/.dockerenv") else "build"
    subprocess.run(["cmake", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", build_dir, "-j"], check=True)


def run_once(binary, domain, mode, size, count, port=9876, unix_path=None):
    if unix_path is None:
        unix_path = os.path.join(tempfile.gettempdir(), f"bench_{os.getpid()}.sock")
    cmd = [
        binary,
        "-d", domain,
        "-m", mode,
        "-s", str(size),
        "-n", str(count),
        "-p", str(port),
        "-u", unix_path,
        "--csv",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr.strip()}", file=sys.stderr)
        return None
    line = result.stdout.strip().split("\n")[-1]
    return dict(zip(CSV_FIELDS, line.split(",")))


def run_conn_test(binary, domain, port=9877, unix_path=None):
    if unix_path is None:
        unix_path = os.path.join(tempfile.gettempdir(), f"bench_conn_{os.getpid()}.sock")
    cmd = [
        binary,
        "-d", domain,
        "-t",
        "-n", str(CONN_TEST_CYCLES),
        "-p", str(port),
        "-u", unix_path,
        "--csv",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr.strip()}", file=sys.stderr)
        return None
    line = result.stdout.strip().split("\n")[-1]
    return dict(zip(CSV_FIELDS, line.split(",")))


def median_run(binary, domain, mode, size, count):
    runs = []
    for i in range(RUNS_PER_CONFIG):
        r = run_once(binary, domain, mode, size, count)
        if r:
            runs.append(r)
        time.sleep(0.1)
    if not runs:
        return None
    keys = ["duration_sec", "throughput_mb_s", "packets_per_sec", "conn_setup_us", "conn_teardown_us"]
    med = dict(runs[0])
    for k in keys:
        vals = [float(r[k]) for r in runs if r.get(k)]
        if vals:
            med[k] = f"{statistics.median(vals):.6f}"
    return med


def collect_env():
    lines = []
    lines.append(f"platform: {platform.platform()}")
    lines.append(f"machine: {platform.machine()}")
    lines.append(f"processor: {platform.processor()}")
    lines.append(f"python: {platform.python_version()}")
    try:
        gcc = subprocess.run(["gcc", "--version"], capture_output=True, text=True)
        lines.append(f"gcc: {gcc.stdout.splitlines()[0]}")
    except Exception:
        lines.append("gcc: not found")
    try:
        cmake = subprocess.run(["cmake", "--version"], capture_output=True, text=True)
        lines.append(f"cmake: {cmake.stdout.splitlines()[0]}")
    except Exception:
        lines.append("cmake: not found")
    return "\n".join(lines)


def main():
    # Navigate to project root (two levels up from lab1/scripts/)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    os.chdir(project_root)

    binary = find_binary()
    if not binary:
        print("Binary not found, building...")
        build()
        binary = find_binary()
    if not binary:
        print("ERROR: could not find or build socket_benchmark", file=sys.stderr)
        sys.exit(1)

    print(f"Using binary: {binary}")
    print(f"Platform: {platform.platform()} ({platform.machine()})")
    print(f"Runs per config: {RUNS_PER_CONFIG}")
    print()

    results_dir = os.path.join(os.getcwd(), "results")
    os.makedirs(results_dir, exist_ok=True)
    csv_path = os.path.join(results_dir, "results.csv")
    env_path = os.path.join(results_dir, "env.txt")

    with open(env_path, "w") as f:
        f.write(collect_env())

    all_rows = []

    # Throughput matrix
    for i, (domain, mode, size, count) in enumerate(MATRIX, 1):
        label = f"{domain.upper()} | {mode:8s} | {size:>6d}B | {count:>7d} pkts"
        print(f"[{i:>2}/{len(MATRIX)}] {label} ... ", end="", flush=True)

        row = median_run(binary, domain, mode, size, count)
        if row:
            all_rows.append(row)
            mbps = float(row.get("throughput_mb_s", 0))
            pkts = float(row.get("packets_per_sec", 0))
            print(f"{mbps:>10.2f} MB/s  {pkts:>12.0f} pkts/s")
        else:
            print("FAILED")

    # Connection latency tests
    for domain in ["unix", "inet"]:
        print(f"\nConn test: {domain.upper()} x{CONN_TEST_CYCLES} ... ", end="", flush=True)
        row = run_conn_test(binary, domain)
        if row:
            all_rows.append(row)
            setup = float(row.get("conn_setup_us", 0))
            teardown = float(row.get("conn_teardown_us", 0))
            print(f"setup={setup:.1f}us  teardown={teardown:.1f}us")
        else:
            print("FAILED")

    # Write CSV
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(all_rows)

    print(f"\nResults written to: {csv_path}")
    print(f"Environment info:  {env_path}")


if __name__ == "__main__":
    main()
