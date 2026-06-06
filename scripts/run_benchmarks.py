#!/usr/bin/env python3
"""
run_benchmarks.py — Automated benchmark runner for the PDC N-Body project.

Runs serial, OpenMP, pthreads, and MPI+OpenMP variants across a matrix of
body counts and thread/process settings, then writes a CSV with:
  implementation, N, steps, parallel_units, time_ms, speedup, efficiency

Usage:
    python scripts/run_benchmarks.py --build-dir build --out results/benchmark.csv
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

# ── Configuration ──────────────────────────────────────────────────────────
# (N, steps) pairs: larger N → more work → better speedup visibility
WORKLOADS = [
    (512,  100),
    (1024, 50),
    (2048, 20),
]

OPENMP_THREADS  = [1, 2, 4, 8]
PTHREAD_THREADS = [1, 2, 4, 8]
MPI_CONFIGS     = [          # (procs, omp_threads_per_proc)
    (1, 1),
    (2, 1),
    (2, 2),
    (4, 1),
    (4, 2),
]

ELAPSED_RE = re.compile(r"ElapsedMs:([0-9]+(?:\.[0-9]+)?)")


def exe(build_dir: Path, name: str) -> str:
    """Return full path to executable, checking .exe extension on Windows."""
    p = build_dir / (name + ".exe")
    if p.exists():
        return str(p)
    p2 = build_dir / name
    if p2.exists():
        return str(p2)
    raise FileNotFoundError(f"Executable not found: {name} in {build_dir}")


def run(cmd: list[str], label: str) -> float:
    """Run command, parse ElapsedMs from stdout, return float ms."""
    print(f"  Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  [WARN] {label} exited {result.returncode}: {result.stderr[:200]}")
        return -1.0
    m = ELAPSED_RE.search(result.stdout)
    if not m:
        print(f"  [WARN] Could not parse ElapsedMs from {label} output")
        return -1.0
    return float(m.group(1))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--out",       default="results/benchmark.csv")
    parser.add_argument("--mpiexec",   default="mpiexec")
    parser.add_argument("--dt",        default="0.001")
    args = parser.parse_args()

    build = Path(args.build_dir)
    out   = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    rows = []

    for (N, steps) in WORKLOADS:
        print(f"\n{'='*60}")
        print(f"Workload: N={N}  steps={steps}")
        print('='*60)

        # ── Serial baseline ────────────────────────────────────────
        t_serial = run(
            [exe(build, "serial_nbody"), str(N), str(steps), args.dt],
            "serial"
        )
        rows.append(dict(implementation="serial", N=N, steps=steps,
                         parallel_units=1, time_ms=t_serial,
                         speedup=1.0, efficiency=1.0))

        if t_serial <= 0:
            print("  Serial failed — skipping parallel variants for this workload")
            continue

        # ── OpenMP ────────────────────────────────────────────────
        for T in OPENMP_THREADS:
            t = run(
                [exe(build, "openmp_nbody"), str(N), str(steps), str(T), args.dt],
                f"openmp T={T}"
            )
            sp = t_serial / t if t > 0 else 0.0
            rows.append(dict(implementation="openmp", N=N, steps=steps,
                             parallel_units=T, time_ms=t,
                             speedup=round(sp, 4),
                             efficiency=round(sp / T, 4)))

        # ── Pthreads ──────────────────────────────────────────────
        for T in PTHREAD_THREADS:
            t = run(
                [exe(build, "pthread_nbody"), str(N), str(steps), str(T), args.dt],
                f"pthreads T={T}"
            )
            sp = t_serial / t if t > 0 else 0.0
            rows.append(dict(implementation="pthreads", N=N, steps=steps,
                             parallel_units=T, time_ms=t,
                             speedup=round(sp, 4),
                             efficiency=round(sp / T, 4)))

        # ── MPI + OpenMP ──────────────────────────────────────────
        mpi_exe = build / "mpi_nbody.exe"
        if not mpi_exe.exists():
            mpi_exe = build / "mpi_nbody"
        if mpi_exe.exists():
            for (procs, omp_t) in MPI_CONFIGS:
                cmd = [
                    args.mpiexec, "-n", str(procs),
                    str(mpi_exe), str(N), str(steps), str(omp_t), args.dt
                ]
                t = run(cmd, f"mpi P={procs} OMP={omp_t}")
                total_units = procs * omp_t
                sp = t_serial / t if t > 0 else 0.0
                rows.append(dict(implementation="mpi_hybrid", N=N, steps=steps,
                                 parallel_units=total_units, time_ms=t,
                                 speedup=round(sp, 4),
                                 efficiency=round(sp / total_units, 4)))

    # ── Write CSV ──────────────────────────────────────────────────
    fieldnames = ["implementation", "N", "steps", "parallel_units",
                  "time_ms", "speedup", "efficiency"]
    with out.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(rows)

    print(f"\nBenchmark CSV written: {out}")
    print(f"Total rows: {len(rows)}")

    # ── Print quick summary table ──────────────────────────────────
    print("\n{:<14} {:>6} {:>6} {:>4} {:>12} {:>8} {:>10}".format(
        "impl", "N", "steps", "P", "time_ms", "speedup", "efficiency"))
    print("-" * 68)
    for r in rows:
        print("{:<14} {:>6} {:>6} {:>4} {:>12.1f} {:>8.3f} {:>10.3f}".format(
            r["implementation"], r["N"], r["steps"], r["parallel_units"],
            r["time_ms"], r["speedup"], r["efficiency"]))


if __name__ == "__main__":
    main()
