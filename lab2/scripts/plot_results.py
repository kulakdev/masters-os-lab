#!/usr/bin/env python3
"""Generate comparison charts for lab2 from results/lab2_results.csv."""

import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

METHOD_ORDER = ["mmap_file", "mmap_anon", "shm", "file", "file_fsync",
                "pipe", "fifo", "socket", "mqueue"]
COLORS = {
    "mmap_file": "#1f77b4", "mmap_anon": "#4c9be8", "shm": "#17becf",
    "file": "#8c564b", "file_fsync": "#c49c94", "pipe": "#2ca02c",
    "fifo": "#98df8a", "socket": "#ff7f0e", "mqueue": "#d62728",
}


def load(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def ordered(methods):
    known = [m for m in METHOD_ORDER if m in methods]
    return known + sorted(m for m in methods if m not in METHOD_ORDER)


def bar_chart(rows, test, value_key, title, ylabel, out_path, logy=True):
    rows = [r for r in rows if r["test"] == test]
    if not rows:
        return
    sizes = sorted({int(r["msg_size_bytes"]) for r in rows})
    methods = ordered({r["method"] for r in rows})

    fig, ax = plt.subplots(figsize=(10, 6))
    width = 0.8 / max(len(sizes), 1)
    for si, size in enumerate(sizes):
        vals, labels = [], []
        for m in methods:
            match = [r for r in rows if r["method"] == m and int(r["msg_size_bytes"]) == size]
            vals.append(float(match[0][value_key]) if match else 0.0)
            labels.append(m)
        xs = [i + si * width for i in range(len(methods))]
        bars = ax.bar(xs, vals, width=width, label=f"{size} B")
        if not logy:
            for x, v in zip(xs, vals):
                if v:
                    ax.text(x, v, f"{v:.0f}", ha="center", va="bottom", fontsize=7)

    ax.set_xticks([i + width * (len(sizes) - 1) / 2 for i in range(len(methods))])
    ax.set_xticklabels(methods, rotation=25, ha="right")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if logy:
        ax.set_yscale("log")
    ax.legend(title="payload")
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"wrote {out_path}")


def latency_chart(rows, out_path):
    rows = [r for r in rows if r["test"] == "latency"]
    if not rows:
        return
    methods = ordered({r["method"] for r in rows})
    vals = [float(next((r["latency_us"] for r in rows if r["method"] == m), 0.0)) for m in methods]

    fig, ax = plt.subplots(figsize=(10, 5))
    bars = ax.bar(methods, vals, color=[COLORS.get(m, "#7f7f7f") for m in methods])
    for b, v in zip(bars, vals):
        if v:
            ax.text(b.get_x() + b.get_width() / 2, v, f"{v:.2f}", ha="center", va="bottom", fontsize=8)
    ax.set_ylabel("one-way latency, us (log)")
    ax.set_title("IPC one-way latency (64 B ping-pong, median)")
    ax.set_yscale("log")
    ax.tick_params(axis="x", rotation=25)
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"wrote {out_path}")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(os.path.dirname(here))
    csv_path = os.path.join(root, "results", "lab2_results.csv")
    out_dir = os.path.join(root, "results")
    if not os.path.isfile(csv_path):
        raise SystemExit(f"missing {csv_path}; run run_benchmarks.py first")

    rows = load(csv_path)
    bar_chart(rows, "throughput", "throughput_mb_s",
              "IPC throughput by payload size (median)",
              "MB/s (log)", os.path.join(out_dir, "lab2_throughput.png"))
    latency_chart(rows, os.path.join(out_dir, "lab2_latency.png"))
    bar_chart(rows, "capacity", "capacity_bytes",
              "IPC channel capacity (msg size 4 KB)",
              "bytes (log)", os.path.join(out_dir, "lab2_capacity.png"))


if __name__ == "__main__":
    main()
