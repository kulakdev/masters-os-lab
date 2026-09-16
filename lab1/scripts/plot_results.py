import csv
from pathlib import Path

import matplotlib.pyplot as plt

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
RESULTS_DIR = PROJECT_ROOT / "results"
RESULTS_DIR.mkdir(exist_ok=True)


def load(name):
    with (RESULTS_DIR / name).open() as f:
        rows = list(csv.DictReader(f))
    data = [r for r in rows if int(r["msg_count"]) != 1000]
    conn = [r for r in rows if int(r["msg_count"]) == 1000]
    return data, conn


def mbps(r):
    return float(r["throughput_mb_s"])


SIZES = [64, 4096, 65536]
SERIES = [("unix", "blocking"), ("unix", "epoll"), ("inet", "blocking"), ("inet", "epoll")]

native, native_conn = load("results_native_macos.csv")
docker, docker_conn = load("results_docker.csv")


def series_values(data, dom, mode):
    return [mbps(r) for r in data
            if r["domain"] == dom and r["io_mode"] == mode
            and int(r["msg_size_bytes"]) in SIZES]


# --- Native throughput vs size, 4 series ---
fig, ax = plt.subplots(figsize=(8, 5))
for dom, mode in SERIES:
    ax.plot(SIZES, series_values(native, dom, mode), marker="o", label=f"{dom}-{mode}")
ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xticks(SIZES)
ax.set_xticklabels(SIZES)
ax.set_xlabel("payload size (bytes)")
ax.set_ylabel("throughput (MB/s)")
ax.set_title("Native macOS — throughput vs payload size")
ax.legend()
ax.grid(True, which="both", alpha=0.3)
plt.tight_layout()
plt.savefig(RESULTS_DIR / "throughput_native.png", dpi=150)
plt.close(fig)

# --- Docker vs Native throughput, faceted by (domain, mode) ---
fig, axes = plt.subplots(2, 2, figsize=(11, 7), sharex=True, sharey=True)
x = range(len(SIZES))
width = 0.35
for (dom, mode), ax in zip(SERIES, axes.flat):
    d = series_values(docker, dom, mode)
    n = series_values(native, dom, mode)
    ax.bar([i - width / 2 for i in x], d, width, label="docker (Linux)")
    ax.bar([i + width / 2 for i in x], n, width, label="native (macOS)")
    ax.set_yscale("log")
    ax.set_title(f"{dom} — {mode}")
    ax.set_xticks(list(x))
    ax.set_xticklabels([f"{s}B" if s < 1024 else f"{s // 1024}KB" for s in SIZES])
    ax.grid(True, axis="y", which="both", alpha=0.3)
axes[0, 0].legend()
axes[1, 0].set_ylabel("throughput (MB/s)")
plt.suptitle("Throughput: Docker (Linux) vs Native (macOS)")
plt.tight_layout()
plt.savefig(RESULTS_DIR / "throughput_comparison.png", dpi=150)
plt.close(fig)


def conn_metrics(rows, dom):
    r = [x for x in rows if x["domain"] == dom][0]
    return float(r["conn_setup_us"]), float(r["conn_teardown_us"])


# --- Native connection latency ---
fig, ax = plt.subplots(figsize=(6, 4))
labels = ["unix", "inet"]
setup = [conn_metrics(native_conn, d)[0] for d in labels]
teardown = [conn_metrics(native_conn, d)[1] for d in labels]
x = range(len(labels))
ax.bar([i - 0.2 for i in x], setup, 0.4, label="setup")
ax.bar([i + 0.2 for i in x], teardown, 0.4, label="teardown")
ax.set_xticks(list(x))
ax.set_xticklabels(labels)
ax.set_ylabel("latency (µs)")
ax.set_title("Native macOS — connection setup/teardown")
ax.legend()
plt.tight_layout()
plt.savefig(RESULTS_DIR / "conn_latency_native.png", dpi=150)
plt.close(fig)

# --- Docker vs Native connection latency, faceted by domain ---
fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
for dom, ax in zip(["unix", "inet"], axes):
    d_setup, d_tear = conn_metrics(docker_conn, dom)
    n_setup, n_tear = conn_metrics(native_conn, dom)
    groups = ["setup", "teardown"]
    docker_vals = [d_setup, d_tear]
    native_vals = [n_setup, n_tear]
    x = range(len(groups))
    ax.bar([i - 0.2 for i in x], docker_vals, 0.4, label="docker (Linux)")
    ax.bar([i + 0.2 for i in x], native_vals, 0.4, label="native (macOS)")
    ax.set_xticks(list(x))
    ax.set_xticklabels(groups)
    ax.set_title(f"{dom}")
    ax.grid(True, axis="y", alpha=0.3)
axes[0].set_ylabel("latency (µs)")
axes[0].legend()
plt.suptitle("Connection latency: Docker (Linux) vs Native (macOS)")
plt.tight_layout()
plt.savefig(RESULTS_DIR / "conn_latency_comparison.png", dpi=150)
plt.close(fig)

print(f"Saved plots to {RESULTS_DIR}")
