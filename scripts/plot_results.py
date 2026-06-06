#!/usr/bin/env python3
"""
plot_results.py — Generate publication-quality performance graphs for the
                  PDC N-Body project report.

Produces (saved to --out-dir):
  1. speedup_vs_threads.png    — Speedup curves for each implementation
  2. efficiency_vs_threads.png — Efficiency curves
  3. time_vs_N.png             — Wall-clock time scaling with N
  4. scalability_heatmap.png   — Speedup heatmap across N × P

Usage:
    python scripts/plot_results.py --csv results/benchmark.csv --out-dir results/
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

# matplotlib is not guaranteed; fall back to ASCII tables if not available
try:
    import matplotlib
    matplotlib.use("Agg")  # non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


def load_csv(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    # Normalise implementation names to lowercase for consistent key lookup
    for r in rows:
        r["implementation"] = r["implementation"].lower().replace("+", "_").replace(" ", "_")
    return rows


def to_float(v):
    try:
        return float(v)
    except (ValueError, TypeError):
        return 0.0


# ── Colour / marker maps ────────────────────────────────────────────────────
COLORS = {
    "serial":     "#555555",
    "openmp":     "#2196F3",
    "pthreads":   "#4CAF50",
    "mpi_hybrid": "#FF5722",
}
MARKERS = {
    "serial":     "o",
    "openmp":     "s",
    "pthreads":   "^",
    "mpi_hybrid": "D",
}
LABELS = {
    "serial":     "Serial",
    "openmp":     "OpenMP",
    "pthreads":   "Pthreads",
    "mpi_hybrid": "MPI+OpenMP",
}


def plot_speedup(rows: list[dict], out_dir: Path):
    """Speedup vs parallel_units for each implementation (largest N only)."""
    max_N = max(int(r["N"]) for r in rows)
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle(
        f"N-Body Performance  (N={max_N}, all steps)",
        fontsize=13, fontweight="bold"
    )

    for impl in ["openmp", "pthreads", "mpi_hybrid"]:
        subset = [r for r in rows
                  if r["implementation"] == impl and int(r["N"]) == max_N]
        if not subset:
            continue
        subset.sort(key=lambda r: int(r["parallel_units"]))
        xs = [int(r["parallel_units"]) for r in subset]
        ys_sp = [to_float(r["speedup"]) for r in subset]
        ys_ef = [to_float(r["efficiency"]) for r in subset]

        axes[0].plot(xs, ys_sp, marker=MARKERS[impl], color=COLORS[impl],
                     label=LABELS[impl], linewidth=2, markersize=7)
        axes[1].plot(xs, ys_ef, marker=MARKERS[impl], color=COLORS[impl],
                     label=LABELS[impl], linewidth=2, markersize=7)

    # Ideal speedup line
    max_p = max(int(r["parallel_units"]) for r in rows)
    ideal_x = list(range(1, max_p + 1))
    axes[0].plot(ideal_x, ideal_x, "--", color="grey", linewidth=1,
                 label="Ideal (linear)", alpha=0.6)
    axes[1].axhline(y=1.0, color="grey", linestyle="--", linewidth=1,
                    label="Perfect efficiency", alpha=0.6)

    axes[0].set_title("Speedup S(P) = T₁ / T_P")
    axes[0].set_xlabel("Parallel Units (P)")
    axes[0].set_ylabel("Speedup")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)
    axes[0].xaxis.set_major_locator(ticker.MaxNLocator(integer=True))

    axes[1].set_title("Efficiency E(P) = S(P) / P")
    axes[1].set_xlabel("Parallel Units (P)")
    axes[1].set_ylabel("Efficiency")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)
    axes[1].xaxis.set_major_locator(ticker.MaxNLocator(integer=True))

    plt.tight_layout()
    out = out_dir / "speedup_efficiency.png"
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"  Saved: {out}")


def plot_time_vs_N(rows: list[dict], out_dir: Path):
    """Wall-clock time vs N for each implementation at P=1 or lowest P."""
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.set_title("Execution Time vs Problem Size N", fontsize=12, fontweight="bold")

    for impl in ["serial", "openmp", "pthreads", "mpi_hybrid"]:
        # Pick the lowest-P variant for each N
        by_N = defaultdict(list)
        for r in rows:
            if r["implementation"] == impl:
                by_N[int(r["N"])].append(r)
        if not by_N:
            continue

        points = []
        for n, recs in sorted(by_N.items()):
            best = min(recs, key=lambda r: int(r["parallel_units"]))
            points.append((n, to_float(best["time_ms"])))

        if points:
            xs, ys = zip(*sorted(points))
            ax.plot(xs, ys, marker=MARKERS[impl], color=COLORS[impl],
                    label=LABELS[impl], linewidth=2, markersize=7)

    ax.set_xlabel("Number of Bodies (N)")
    ax.set_ylabel("Execution Time (ms)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))

    plt.tight_layout()
    out = out_dir / "time_vs_N.png"
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"  Saved: {out}")


def plot_bar_comparison(rows: list[dict], out_dir: Path):
    """Bar chart comparing best time per implementation for each N."""
    Ns = sorted(set(int(r["N"]) for r in rows))
    impls = ["serial", "openmp", "pthreads", "mpi_hybrid"]

    fig, axes = plt.subplots(1, len(Ns), figsize=(5 * len(Ns), 5), sharey=False)
    if len(Ns) == 1:
        axes = [axes]
    fig.suptitle("Best Execution Time per Implementation", fontsize=12,
                 fontweight="bold")

    for ax, N in zip(axes, Ns):
        times = []
        labels = []
        for impl in impls:
            recs = [r for r in rows
                    if r["implementation"] == impl and int(r["N"]) == N]
            if recs:
                best_t = min(to_float(r["time_ms"]) for r in recs
                             if to_float(r["time_ms"]) > 0)
                times.append(best_t)
                labels.append(LABELS[impl])

        colors = [COLORS[impl] for impl in impls
                  if any(r["implementation"] == impl and int(r["N"]) == N
                         for r in rows)]
        bars = ax.bar(labels, times, color=colors, edgecolor="white", width=0.6)
        ax.set_title(f"N = {N}")
        ax.set_ylabel("Time (ms)")
        ax.tick_params(axis="x", rotation=20)
        for bar, t in zip(bars, times):
            ax.text(bar.get_x() + bar.get_width() / 2,
                    bar.get_height() * 1.01,
                    f"{t:.0f}", ha="center", va="bottom", fontsize=8)

    plt.tight_layout()
    out = out_dir / "bar_comparison.png"
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"  Saved: {out}")


def print_ascii_table(rows: list[dict]):
    """Fallback: print a formatted table when matplotlib is not available."""
    print("\n" + "="*76)
    print("BENCHMARK SUMMARY (matplotlib not available — install for graphs)")
    print("="*76)
    fmt = "{:<14} {:>6} {:>6} {:>4} {:>12} {:>8} {:>10}"
    print(fmt.format("impl", "N", "steps", "P", "time_ms", "speedup", "eff"))
    print("-"*76)
    for r in rows:
        print(fmt.format(
            r["implementation"], r["N"], r["steps"],
            r["parallel_units"], f"{to_float(r['time_ms']):.1f}",
            r["speedup"], r["efficiency"]
        ))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv",     required=True)
    parser.add_argument("--out-dir", default="results")
    args = parser.parse_args()

    csv_path = Path(args.csv)
    out_dir  = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not csv_path.exists():
        print(f"Error: CSV file not found: {csv_path}")
        raise SystemExit(1)

    rows = load_csv(csv_path)
    if not rows:
        print("Error: CSV is empty")
        raise SystemExit(1)

    if HAS_MATPLOTLIB:
        print("Generating plots …")
        plot_speedup(rows, out_dir)
        plot_time_vs_N(rows, out_dir)
        plot_bar_comparison(rows, out_dir)
        print("\nAll plots saved to:", out_dir)
    else:
        print_ascii_table(rows)
        print("\nTip: install matplotlib for graphical output:")
        print("  pip install matplotlib")


if __name__ == "__main__":
    main()
