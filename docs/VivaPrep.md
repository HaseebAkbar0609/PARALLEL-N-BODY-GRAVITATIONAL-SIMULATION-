# PDC Project — Viva Preparation Guide

## Quick-fire Q&A for presentation day

### Problem & Motivation

**Q: Why did you choose N-body simulation?**
A: It is an O(n²) compute-intensive kernel used in astrophysics and molecular
   dynamics. The dominant cost — computing forces between all pairs of bodies —
   is perfectly suited for parallelism because each row i is independent of all
   other rows. This gives genuine, measurable speedup, not just a toy demo.

**Q: How is this a "real-world" problem?**
A: N-body simulation is used in cosmological simulations (dark matter), protein
   folding, game engine physics, and fluid particle systems. NASA and ESA run
   variants of this algorithm on supercomputers for satellite trajectory modelling.

---

### OpenMP

**Q: How did you parallelise the force computation with OpenMP?**
A: We used `#pragma omp parallel for schedule(dynamic, 4)` over the outer loop
   (body index i). Each thread computes forces on its assigned bodies into a
   thread-local array, then merges under `#pragma omp critical`. Dynamic
   scheduling balances the uneven work distribution.

**Q: Why thread-local arrays instead of atomic operations?**
A: Atomic operations add overhead on every write. Thread-local arrays accumulate
   freely with zero contention; the single merge at the end is O(n×T) which is
   negligible compared to the O(n²) compute.

**Q: How did you prevent race conditions?**
A: Only one thread ever writes to its own private fx_buf[tid][i]. Global bodies[]
   are read-only during the compute phase and written only after the critical
   section merge is complete.

---

### POSIX Threads

**Q: Why did you use barriers instead of a mutex per step?**
A: A mutex would only protect one critical section at a time. Barriers synchronise
   ALL threads at once — ensuring every thread finishes Phase 2 (compute) before
   ANY thread starts Phase 3 (merge), which is the correct ordering guarantee.

**Q: How many barriers are there and why?**
A: Three barriers per step:
   1. `barrier_compute_done` — ensures all force computations are finished
   2. `barrier_merge_done`   — ensures the global merge is complete before integration
   3. `barrier_step_done`    — signals workers the integration is done, next step can start

**Q: Could this deadlock?**
A: No. Every thread (workers + main) always reaches each barrier in the same order.
   No thread holds any mutex when waiting at a barrier, so there is no cycle in the
   resource dependency graph.

**Q: Why create threads once before the loop instead of per step?**
A: `pthread_create` has non-trivial overhead (~microseconds per thread). Creating T
   threads once and reusing them via barriers reduces fixed overhead from O(steps × T)
   to O(T) — crucial for short steps with large T.

---

### MPI + OpenMP Hybrid

**Q: How is work divided across MPI processes?**
A: Striped row assignment: rank r owns rows r, r+P, r+2P, … where P is the number
   of processes. This is static and perfectly balanced for O(n²) work.

**Q: What MPI communication is used and when?**
A: `MPI_Allreduce` (MPI_SUM) is called once per step, on three arrays of n doubles
   (fx, fy, fz). Each rank contributes its partial forces; all ranks receive the
   complete summed force vector. No point-to-point messages are used.

**Q: What is the communication overhead?**
A: 3 × n × 8 bytes = 24n bytes per step. For n=1024, that is ~25 KB per step.
   This is O(n) overhead versus O(n²) compute, so it becomes negligible for large n.

**Q: Can this run on one laptop?**
A: Yes. MPI launches multiple local processes on the same CPU. `mpiexec -n 4` spawns
   4 processes that communicate via shared memory (using Windows shared memory
   transport), not a network. This demonstrates the distributed-memory programming
   model on a single machine.

---

### Performance Analysis

**Q: Why is the speedup not linear (ideal)?**
A: Amdahl's Law: the serial fraction of the code (integration, barriers, merge) does
   not scale. Additionally:
   - Thread creation/barrier overhead adds fixed cost
   - Memory bandwidth becomes the bottleneck at high thread counts
   - Cache invalidation from multiple threads accessing the bodies[] array

**Q: What is Amdahl's Law?**
A: S(P) = 1 / (f + (1-f)/P), where f is the serial fraction.
   If 5% of the code is serial, the theoretical maximum speedup is 20×,
   regardless of how many processors you add.

**Q: How did you verify correctness across all implementations?**
A: We compute total mechanical energy (KE + PE) before and after each run using
   the same deterministic initial conditions (fixed seed = 42). All implementations
   must produce `EnergyError < 1e-4` and print `[PASS]`. Since energy is a global
   scalar, any numerical discrepancy from incorrect parallelism would show up here.

---

### Code Quality

**Q: How is the code modularised?**
A: 
- `nbody.hpp / nbody.cpp` — physics engine (serial reference, shared by all)
- `timer.hpp` — portable high-resolution timer
- `serial_nbody.cpp` — baseline executable
- `openmp_nbody.cpp` — OpenMP parallel version
- `pthread_nbody.cpp` — Pthreads version
- `mpi_nbody.cpp` — MPI+OpenMP hybrid version
- `scripts/` — dataset generation, benchmarking, plotting (Python)

**Q: How do you build the project?**
A: `bash scripts/build.sh` inside the MSYS2 MinGW64 shell. It compiles all four
   executables with GCC 16 using `-O2 -std=c++17 -fopenmp -lpthread -lmsmpi`.

---

## Individual Contribution Statements (fill in before viva)

| Member | Contribution |
|---|---|
| Member 1 | `nbody.cpp` core physics engine, `serial_nbody.cpp`, energy verification |
| Member 2 | `openmp_nbody.cpp`, `pthread_nbody.cpp`, synchronisation analysis |
| Member 3 | `mpi_nbody.cpp`, benchmark scripts, performance plots, report |

---

## Demo Command Sequence (paste into terminal during demo)

```bash
# Open MSYS2 MinGW64, then:
cd "/c/Users/Dr.Tech/OneDrive/Desktop/PDC Lab/pdc-parallel-nbody"

# 1. Serial
./build/serial_nbody.exe 1024 50

# 2. OpenMP — show speedup
./build/openmp_nbody.exe 1024 50 2
./build/openmp_nbody.exe 1024 50 4
./build/openmp_nbody.exe 1024 50 8

# 3. Pthreads
./build/pthread_nbody.exe 1024 50 4

# 4. MPI hybrid
mpiexec -n 4 ./build/mpi_nbody.exe 1024 50 2

# 5. Full benchmark
python scripts/run_benchmarks.py --build-dir build --out results/benchmark.csv
python scripts/plot_results.py --csv results/benchmark.csv --out-dir results/
```
