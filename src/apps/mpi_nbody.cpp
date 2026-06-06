/*
 * mpi_nbody.cpp — MPI + OpenMP Hybrid N-Body Gravitational Simulation.
 *
 * PDC Concepts Demonstrated
 * ─────────────────────────
 *   • MPI_Init / MPI_Finalize          — process lifecycle
 *   • MPI_Comm_rank / MPI_Comm_size    — process identity
 *   • MPI_Allgather                    — collective: share all positions
 *   • MPI_Reduce (MPI_SUM)             — collective: sum forces across ranks
 *   • MPI_Wtime + MPI_Barrier          — accurate distributed timing
 *   • MPI_Bcast                        — broadcast energy from rank 0
 *   • OpenMP (#pragma omp parallel for) — intra-node thread parallelism
 *
 * Distributed Workload Strategy
 * ──────────────────────────────
 *   With P MPI processes and N bodies:
 *
 *   Each process owns a STRIPE of rows:
 *     rank r  →  rows r, r+P, r+2P, ...  (striped / round-robin)
 *
 *   Every rank has a FULL copy of the body array (position data),
 *   so it can compute forces without inter-process communication
 *   during the hot loop.
 *
 *   After force computation, each rank has correct forces for its
 *   owned rows but zero for others.  We use MPI_Allreduce (sum) to
 *   merge force arrays across all ranks so every rank gets the global
 *   force array — then each rank independently integrates.
 *
 * Communication Analysis
 *   • MPI_Allreduce per step: 3 arrays × n doubles × sizeof(double)
 *     = 24n bytes per step (scales linearly with n, not n²).
 *   • No point-to-point messages in the hot path → no deadlock risk.
 *   • MPI_Barrier is only used for accurate timing.
 *
 * Usage: mpirun -np <P> mpi_nbody <N> <steps> <omp_threads> [dt] [out.csv]
 *   P           : MPI process count  (e.g. 2 or 4)
 *   omp_threads : OpenMP threads per process (typically CPU_cores / P)
 */

#include "nbody.hpp"
#include "timer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

/* MSMPI on MinGW requires MSMPI_NO_SAL before mpi.h to suppress SAL annotations */
#define MSMPI_NO_SAL
#ifndef _WIN64
#  define _WIN64
#endif
#include <mpi.h>

#ifdef _OPENMP
#  include <omp.h>
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * Hybrid force kernel: each MPI rank handles its stripe of rows;
 * within each row OpenMP parallelises the column (j) loop.
 * ───────────────────────────────────────────────────────────────────────── */
static void computeForceHybrid(
    pdc::Body* bodies,  /* full body array (read-only for positions) */
    double*    fx_local, double* fy_local, double* fz_local, /* write */
    int n, int rank, int world_size, int omp_threads
) {
    (void)omp_threads;
#ifdef _OPENMP
    omp_set_num_threads(omp_threads);
#endif

    /* Zero the local force arrays */
    std::memset(fx_local, 0, static_cast<std::size_t>(n) * sizeof(double));
    std::memset(fy_local, 0, static_cast<std::size_t>(n) * sizeof(double));
    std::memset(fz_local, 0, static_cast<std::size_t>(n) * sizeof(double));

    /*
     * Each MPI rank owns striped rows: rank, rank+world_size, ...
     * Within each owned row i, all j are computed via OpenMP.
     */
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 4) default(none) \
    shared(bodies, fx_local, fy_local, fz_local, n, rank, world_size)
#endif
    for (int i = rank; i < n; i += world_size) {
        double acc_fx = 0.0, acc_fy = 0.0, acc_fz = 0.0;

        for (int j = 0; j < n; ++j) {
            if (i == j) continue;

            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;

            double distSq  = dx*dx + dy*dy + dz*dz
                           + pdc::SOFTENING * pdc::SOFTENING;
            double dist    = sqrt(distSq);
            double distCub = distSq * dist;

            double F = pdc::GRAVITY
                     * bodies[i].mass * bodies[j].mass / distCub;

            acc_fx += F * dx;
            acc_fy += F * dy;
            acc_fz += F * dz;
        }

        fx_local[i] = acc_fx;
        fy_local[i] = acc_fy;
        fz_local[i] = acc_fz;
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 * main
 * ───────────────────────────────────────────────────────────────────────── */
int main(int argc, char** argv) {
    /* MPI_THREAD_FUNNELED: only the main thread makes MPI calls (OpenMP on workers) */
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank = 0, world_size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc < 4) {
        if (rank == 0) {
            std::fprintf(stderr,
                "Usage: mpirun -np <P> mpi_nbody <N> <steps> <omp_threads> [dt] [out.csv]\n"
                "  P           : MPI processes\n"
                "  N           : number of bodies\n"
                "  steps       : simulation steps\n"
                "  omp_threads : OpenMP threads per process\n"
                "  dt          : time step (default 0.001)\n"
            );
        }
        MPI_Finalize();
        return 1;
    }

    int    N           = std::atoi(argv[1]);
    int    steps       = std::atoi(argv[2]);
    int    omp_threads = std::atoi(argv[3]);
    double dt          = (argc >= 5) ? std::atof(argv[4]) : pdc::DEFAULT_DT;
    const char* csv_out = (argc >= 6) ? argv[5] : nullptr;

    if (rank == 0) {
        std::printf("[MPI+OpenMP N-Body] N=%d  steps=%d  P=%d  OMP=%d  dt=%.4f\n",
                    N, steps, world_size, omp_threads, dt);
    }

    /* ── Every rank initialises the same body array ────────────── */
    pdc::Body* bodies = pdc::allocBodies(N);
    pdc::initRandomBodies(bodies, N, 42u);   /* same seed → identical state */

    double energy0 = 0.0;
    if (rank == 0) {
        energy0 = pdc::computeTotalEnergy(bodies, N);
        std::printf("Initial total energy: %.6e J\n", energy0);
    }

    /* Per-rank local force arrays */
    std::vector<double> fx_local(static_cast<std::size_t>(N), 0.0);
    std::vector<double> fy_local(static_cast<std::size_t>(N), 0.0);
    std::vector<double> fz_local(static_cast<std::size_t>(N), 0.0);

    /* Global force arrays (after MPI_Allreduce) */
    std::vector<double> fx_global(static_cast<std::size_t>(N), 0.0);
    std::vector<double> fy_global(static_cast<std::size_t>(N), 0.0);
    std::vector<double> fz_global(static_cast<std::size_t>(N), 0.0);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    for (int s = 0; s < steps; ++s) {
        /* Phase 1: Each rank resets its local force buffer */
        std::fill(fx_local.begin(), fx_local.end(), 0.0);
        std::fill(fy_local.begin(), fy_local.end(), 0.0);
        std::fill(fz_local.begin(), fz_local.end(), 0.0);

        /* Phase 2: Compute forces for owned rows (MPI+OpenMP) */
        computeForceHybrid(
            bodies,
            fx_local.data(), fy_local.data(), fz_local.data(),
            N, rank, world_size, omp_threads
        );

        /* Phase 3: MPI_Allreduce — sum all local force arrays
           so every rank has the complete force vector */
        MPI_Allreduce(fx_local.data(), fx_global.data(), N,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(fy_local.data(), fy_global.data(), N,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(fz_local.data(), fz_global.data(), N,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        /* Phase 4: Copy global forces into bodies[] and integrate */
        for (int i = 0; i < N; ++i) {
            bodies[i].fx = fx_global[static_cast<std::size_t>(i)];
            bodies[i].fy = fy_global[static_cast<std::size_t>(i)];
            bodies[i].fz = fz_global[static_cast<std::size_t>(i)];
        }
        pdc::integrateStep(bodies, N, dt);   /* all ranks do this identically */

        if (rank == 0 && ((s + 1) % 50 == 0 || (s + 1) == steps)) {
            std::printf("  Step %4d / %4d\r", s + 1, steps);
            std::fflush(stdout);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end     = MPI_Wtime();
    double elapsed_s = t_end - t_start;
    double elapsed_ms = elapsed_s * 1000.0;

    if (rank == 0) {
        std::printf("\n");
        double energyF = pdc::computeTotalEnergy(bodies, N);
        int total_units = world_size * omp_threads;
        pdc::printResult("MPI+OpenMP", N, steps, total_units,
                         elapsed_ms, energy0, energyF);
        std::printf("ElapsedMs:%.3f\n", elapsed_ms);

        if (csv_out) {
            pdc::writeBodiesCSV(bodies, N, csv_out);
            std::printf("Final positions written to: %s\n", csv_out);
        }
    }

    pdc::freeBodies(bodies);
    MPI_Finalize();
    return 0;
}
