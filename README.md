# PDC Parallel N-Body Gravitational Simulation

**Topic:** Parallel N-Body Gravitational Simulation
**Domain:** Scientific Computation / High-Performance Computing
**PDC Components:** OpenMP · POSIX Threads · MPI + OpenMP Hybrid · Performance Analysis

---

## What this project does

Simulates the gravitational interaction of N massive bodies in 3D space.
At each time step, every body exerts a gravitational force on every other body (O(n²) work).
This is one of the most studied computationally intensive kernels in HPC — used in
astrophysics, molecular dynamics, and game physics.

The **same problem** is solved four ways:

| Executable | Technology | PDC concept |
|---|---|---|
| `serial_nbody` | Plain C++ | Baseline — no parallelism |
| `openmp_nbody` | OpenMP | Shared-memory parallel loops, thread-local buffers, dynamic scheduling |
| `pthread_nbody` | POSIX Threads | Manual thread lifecycle, barriers, mutexes, producer-consumer pattern |
| `mpi_nbody` | MPI + OpenMP | Distributed memory, MPI_Allreduce collective, intra-node OpenMP |

---

## Can this run on one laptop?

**Yes.** All four versions run on a single Windows laptop using MSYS2 MinGW64.
MPI runs multiple local **processes** on the same machine — it simulates distributed
computing without needing multiple physical nodes.

---

## Project Structure

```
pdc-parallel-nbody/
├── src/
│   ├── common/
│   │   ├── nbody.hpp          ← physics data structures + API
│   │   ├── nbody.cpp          ← core physics engine (serial reference)
│   │   └── timer.hpp          ← portable wall-clock timer
│   └── apps/
│       ├── serial_nbody.cpp   ← serial baseline
│       ├── openmp_nbody.cpp   ← OpenMP parallel version
│       ├── pthread_nbody.cpp  ← POSIX threads version
│       └── mpi_nbody.cpp      ← MPI + OpenMP hybrid version
├── scripts/
│   ├── build.sh               ← one-shot build script (MSYS2)
│   ├── run_benchmarks.py      ← automated timing + speedup/efficiency CSV
│   └── plot_results.py        ← publication-quality performance graphs
├── results/                   ← benchmark CSV + generated plots
├── docs/
│   ├── ProjectPlan.md         ← rubric coverage + team assignment
│   ├── Report.md              ← full project report (fill in screenshots)
│   └── VivaPrep.md            ← viva Q&A guide
├── Makefile                   ← build & run targets
└── README.md                  ← this file
```

---

## Setup (One-Time, MSYS2 Already Installed)

Open **MSYS2 MinGW64** from the Start Menu, then:

```bash
# Navigate to project
cd "/c/Users/Dr.Tech/OneDrive/Desktop/PDC Lab/pdc-parallel-nbody"

# Build everything
bash scripts/build.sh
```

That's it. The script builds all four executables into the `build/` folder.

---

## Running the Programs

```bash
# Serial
./build/serial_nbody.exe 512 100

# OpenMP — 4 threads
./build/openmp_nbody.exe 512 100 4

# Pthreads — 4 threads
./build/pthread_nbody.exe 512 100 4

# MPI + OpenMP — 4 processes × 2 OMP threads each
mpiexec -n 4 ./build/mpi_nbody.exe 512 100 2
```

### Running from PowerShell (Windows — no MSYS2 required)

```powershell
# One-time PATH setup (required for MinGW DLLs + msmpi.dll)
$env:PATH = "C:\msys64\mingw64\bin;C:\Program Files\Microsoft MPI\Bin;" + $env:PATH

# Navigate to project
cd "C:\Users\Dr.Tech\OneDrive\Desktop\PDC Lab\pdc-parallel-nbody"

# Run any executable
.\build\serial_nbody.exe 1024 50
.\build\openmp_nbody.exe 1024 50 4
.\build\pthread_nbody.exe 1024 50 4
& "C:\Program Files\Microsoft MPI\Bin\mpiexec.exe" -n 2 .\build\mpi_nbody.exe 1024 50 2

# Run the pre-built MPI benchmark script
powershell -ExecutionPolicy Bypass -File .\scripts\run_mpi_bench.ps1
```

Arguments: `<N>  <steps>  [<threads>]  [dt]  [output.csv]`

---

## Running the Full Benchmark

```bash
python scripts/run_benchmarks.py --build-dir build --out results/benchmark.csv
python scripts/plot_results.py   --csv results/benchmark.csv --out-dir results/
```

This produces:
- `results/benchmark.csv` — full timing table with speedup and efficiency
- `results/speedup_efficiency.png`
- `results/time_vs_N.png`
- `results/bar_comparison.png`

---

## Performance Model

```
Speedup   S(P) = T_serial / T_P
Efficiency E(P) = S(P) / P
```

Expected results for large N (≥ 1024):

| Implementation | P | Typical Speedup |
|---|---|---|
| OpenMP | 4 | ~3.0× |
| Pthreads | 4 | ~2.8× |
| MPI+OpenMP | 4 | ~2.5× (overhead from Allreduce) |

