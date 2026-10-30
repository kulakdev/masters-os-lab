#!/usr/bin/env python3
"""Run the lab3 cache benchmark suite and store results into results/lab3_results.csv."""

import csv
import os
import platform
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BINARY_CANDIDATES = [
    "build/lab3/cache_benchmark",
    "build-docker/lab3/cache_benchmark",
    "../build/lab3/cache_benchmark",
    "../build-docker/lab3/cache_benchmark",
]
OUTPUT = os.path.join(ROOT, "results", "lab3_results.csv")

BENCHMARKS = ["mem", "stride", "latency", "matmul", "false-sharing", "race"]


def find_binary():
    for rel in BINARY_CANDIDATES:
        path = os.path.abspath(rel)
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None


def build():
    build_dir = "build-docker" if os.path.exists("/.dockerenv") else "build"
    subprocess.run(["cmake", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"], check=True)
    subprocess.run(["cmake", "--build", build_dir, "--target", "cache_benchmark", "-j"],
                   check=True)


def main():
    binary = find_binary()
    if binary is None:
        build()
        binary = find_binary()
    if binary is None:
        sys.exit("cache_benchmark binary not found; run cmake --build build first")

    rows = [["benchmark", "parameter", "value", "unit"]]
    for name in BENCHMARKS:
        print(f"==> {name}", file=sys.stderr)
        out = subprocess.run([binary, name], check=True, capture_output=True, text=True).stdout
        for line in out.splitlines():
            line = line.strip()
            if not line or line.startswith("benchmark,"):
                continue
            rows.append(line.split(","))

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["platform", platform.platform(), "machine", platform.machine()])
        writer.writerows(rows)

    print(f"wrote {OUTPUT} ({len(rows) - 1} measurements)", file=sys.stderr)


if __name__ == "__main__":
    main()
