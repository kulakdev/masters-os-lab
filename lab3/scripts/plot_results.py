#!/usr/bin/env python3
"""Generate lab3 report charts from results/lab3_results.csv into lab3/report/."""

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent.parent
CSV = ROOT / "results" / "lab3_results.csv"
OUT = ROOT / "lab3" / "report"


def load():
    d = {}
    with CSV.open() as f:
        rows = list(csv.reader(f))
    for r in rows[2:]:
        d[(r[0], r[1], r[3])] = float(r[2])
    return d


def series(d, bench, unit, prefix):
    items = [(k[1], v) for k, v in d.items() if k[0] == bench and k[2] == unit and k[1].startswith(prefix)]
    return [i[0][len(prefix):] for i in items], [i[1] for i in items]


def bars(ax, labels, vals, title, ylabel, log=False):
    b = ax.bar(labels, vals, color="#4c72b0")
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    if log:
        ax.set_yscale("log")
    for r, v in zip(b, vals):
        ax.annotate(f"{v:.3g}", (r.get_x() + r.get_width() / 2, v), ha="center", va="bottom", fontsize=8)


def main():
    d = load()

    labels, vals = series(d, "latency", "ns_per_load", "workset_")
    fig, ax = plt.subplots(figsize=(9, 4))
    ax.plot(labels, vals, "o-")
    ax.set_yscale("log")
    ax.set_title("Latency vs working set (pointer chase)")
    ax.set_ylabel("ns per load")
    ax.set_xlabel("working set")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(OUT / "latency.png", dpi=130)

    labels, vals = series(d, "stride", "ns_per_access", "stride_")
    fig, ax = plt.subplots(figsize=(9, 4))
    bars(ax, labels, vals, "Stride sweep", "ns per access")
    plt.setp(ax.get_xticklabels(), rotation=45)
    fig.tight_layout()
    fig.savefig(OUT / "stride.png", dpi=130)

    fig, axes = plt.subplots(1, 3, figsize=(14, 4))
    names = ["l1_32KB", "l2_4MB", "dram_64MB"]
    x = range(len(names))
    seq = [d[("mem", f"sequential_{n}", "ns_per_access")] for n in names]
    rnd = [d[("mem", f"random_{n}", "ns_per_access")] for n in names]
    axes[0].bar([i - 0.2 for i in x], seq, 0.4, label="sequential")
    axes[0].bar([i + 0.2 for i in x], rnd, 0.4, label="random")
    axes[0].set_xticks(list(x), names)
    axes[0].set_title("Sequential vs random")
    axes[0].set_ylabel("ns per access")
    axes[0].legend()
    for ax, n in zip(axes[1:], (512, 1024)):
        vs = ["ijk", "ikj", "blocked64"]
        bars(ax, vs, [d[("matmul", f"{v}_N{n}", "GFLOP_per_s")] for v in vs], f"matmul N={n}", "GFLOP/s")
    fig.tight_layout()
    fig.savefig(OUT / "mem_matmul.png", dpi=130)

    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    fs = ["packed_volatile", "padded_volatile", "packed_atomic_relaxed"]
    bars(axes[0], fs, [d[("false-sharing", v, "ms")] for v in fs], "False sharing", "ms", log=True)
    rc = ["volatile_no_sync", "atomic_relaxed", "atomic_seqcst", "mutex", "local_then_reduce"]
    bars(axes[1], rc, [d[("race", v, "ms")] for v in rc], "Race variants", "ms", log=True)
    plt.setp(axes[0].get_xticklabels(), rotation=15)
    plt.setp(axes[1].get_xticklabels(), rotation=15)
    fig.tight_layout()
    fig.savefig(OUT / "sharing_race.png", dpi=130)
    print("charts written to", OUT)


if __name__ == "__main__":
    main()
