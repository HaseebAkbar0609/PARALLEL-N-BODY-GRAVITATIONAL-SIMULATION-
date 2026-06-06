# Project Plan — Rubric Coverage Map

## Topic: Parallel N-Body Gravitational Simulation

---

## Rubric Criteria Evidence Map

| Criterion | Max | Evidence in this project |
|---|---|---|
| Problem Selection & Proposal | 4 | Real astrophysics/HPC problem. O(n²) complexity gives strong justification for parallelism. |
| PDC Relevance | 4 | 3 PDC components: OpenMP (shared memory), Pthreads (manual threads), MPI+OpenMP (distributed memory). All chosen specifically because force computation is embarrassingly parallel. |
| Implementation / Coding | 6 | 4 working executables (serial + 3 parallel). Energy conservation tested. Modular code. Self-written from scratch. |
| Integration of Two PDC Components | 4 | 3 components meaningfully integrated: OpenMP in openmp_nbody, Pthreads in pthread_nbody, MPI+OpenMP hybrid in mpi_nbody. |
| Workload Division / Sync / Comm | 3 | Striped row decomposition, thread-local force buffers, 3-barrier pthread coordination, MPI_Allreduce collective. All explicitly documented. |
| Testing & Performance Evaluation | 4 | Automated benchmark script runs all configs. CSV output with speedup and efficiency. 3 input sizes × multiple P values. |
| Code Quality & Structure | 3 | Separate common library, apps, scripts. Every function has a block comment. -Wall -Wextra clean build. |
| Output Clarity / Result Presentation | 2 | Formatted results table printed on every run. Python plots: speedup, efficiency, time vs N, bar comparison. |
| Report Quality | 5 | Full 14-section report in docs/Report.md with physics background, algorithms, synchronisation analysis, limitations, ethics. |
| Ethics & Professional Practice | 2 | Original work declared. Honest performance reporting (including cases where parallel is slower for small N). Limitations documented. |
| Presentation & Viva | 3 | docs/VivaPrep.md with 15+ Q&As. Demo script ready. Individual contribution template. |
| **Total** | **40** | **Full coverage** |

---

## PDC Components — Minimum Requirement (need 2, have 3)

1. **OpenMP** (Component 4 from rubric)
   - Parallel for loop over force computation
   - Dynamic scheduling for load balancing
   - Thread-local accumulators (race-condition prevention)
   - Critical section for final merge

2. **POSIX Threads** (Component 3 from rubric)
   - pthread_create / pthread_join lifecycle
   - pthread_barrier_wait (3 barriers per step)
   - pthread_mutex_t (force merge protection)
   - Persistent thread pool (no per-step creation overhead)

3. **MPI + OpenMP Hybrid** (Component 2 from rubric)
   - MPI_Allreduce collective communication
   - MPI_Init_thread (thread-safe MPI)
   - MPI_Barrier for accurate distributed timing
   - Striped row decomposition across ranks

4. **Performance Analysis** (Component 6 from rubric — mandatory for all)
   - Execution time (all 4 implementations)
   - Speedup S(P) = T_serial / T_P
   - Efficiency E(P) = S(P) / P
   - Scalability across N = 512, 1024, 2048
   - Scalability across P = 1, 2, 4, 8
   - Communication overhead analysis (MPI)
   - Synchronisation cost analysis (barriers, mutex)

---

## Team Assignment (3 Members)

### Member 1 — Physics Core + Serial
- `src/common/nbody.hpp` — data structures and API
- `src/common/nbody.cpp` — physics engine (force, integrate, energy)
- `src/common/timer.hpp` — portable timer
- `src/apps/serial_nbody.cpp` — serial baseline
- Correctness verification via energy conservation

### Member 2 — Shared-Memory Parallelism
- `src/apps/openmp_nbody.cpp` — OpenMP implementation
- `src/apps/pthread_nbody.cpp` — Pthreads implementation
- Synchronisation analysis section in report

### Member 3 — Distributed + Tooling + Docs
- `src/apps/mpi_nbody.cpp` — MPI+OpenMP hybrid
- `scripts/run_benchmarks.py` — benchmark automation
- `scripts/plot_results.py` — result visualisation
- `docs/Report.md` — full report
- Presentation slides preparation

---

## Daily Milestone Plan

| Day | Task | Owner |
|---|---|---|
| 1 | Set up MSYS2, GCC, MPI | All |
| 2 | Physics core + serial impl | Member 1 |
| 3 | OpenMP implementation | Member 2 |
| 4 | Pthreads implementation | Member 2 |
| 5 | MPI hybrid implementation | Member 3 |
| 6 | Benchmark + integration testing | All |
| 7 | Benchmark runs, graphs, report write-up | Member 3 |
| 8 | Report review, viva prep, rehearsal | All |
