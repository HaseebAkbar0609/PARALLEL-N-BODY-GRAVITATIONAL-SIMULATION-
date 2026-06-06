/*
 * openmp_nbody.cpp — OpenMP Parallel N-Body Gravitational Simulation.
 *
 * PDC Concepts Demonstrated
 * ─────────────────────────
 *   • #pragma omp parallel for  — distribute force-computation loop across threads
 *   • schedule(static)          — static work division (rows are equal-cost since
 *                                  we use the full O(n²) loop, not Newton's 3rd)
 *   • Race-free direct write    — each thread writes ONLY to bodies[i].fx for its
 *                                  assigned i; no cross-thread conflicts, no locks.
 *   • omp_set_num_threads()     — runtime control of thread count
 *   • Speedup & efficiency       — measured vs the serial baseline automatically.
 *
 * Race-condition analysis
 * ────────────────────────
 *   Using the FULL O(n²) kernel (all j ≠ i), each outer-loop row i writes only
 *   to bodies[i].fx/fy/fz.  With static scheduling, OpenMP assigns disjoint i
 *   ranges to each thread.  Therefore NO two threads ever write to the same
 *   memory location — the loop is entirely lock-free with correct results.
 *
 * Performance design choices
 *   • Scalar accumulation (acc_fx/fy/fz per row) reduces write traffic.
 *   • Bodies array stays in cache (read-only from j side, write from i side).
 *   • No per-step heap allocation — buffers are embedded in each call as scalars.
 *
 * Usage:  openmp_nbody  <N>  <steps>  <threads>  [dt]  [output_csv]
 */

#include "nbody.hpp"
#include "timer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef _OPENMP
#  include <omp.h>
#endif

/*
 * computeForceOpenMP — lock-free O(n²) force kernel.
 *
 * Each thread i in [chunk_start, chunk_end) computes the total force on
 * body i from all other bodies j, accumulates into scalars, then writes the
 * result directly to bodies[i].fx/fy/fz.  Because threads own disjoint i
 * ranges there are no write-write conflicts — no atomics, no mutexes needed.
 */
static void computeForceOpenMP(pdc::Body* bodies, int n) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int i = 0; i < n; ++i) {
        double acc_fx = 0.0, acc_fy = 0.0, acc_fz = 0.0;
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;

            double distSq  = dx*dx + dy*dy + dz*dz
                           + pdc::SOFTENING * pdc::SOFTENING;
            double dist    = std::sqrt(distSq);
            double distCub = distSq * dist;

            double F = pdc::GRAVITY * bodies[i].mass * bodies[j].mass / distCub;

            acc_fx += F * dx;
            acc_fy += F * dy;
            acc_fz += F * dz;
        }
        /* Direct write — safe because only this thread writes to index i */
        bodies[i].fx = acc_fx;
        bodies[i].fy = acc_fy;
        bodies[i].fz = acc_fz;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "Usage: openmp_nbody <N> <steps> <threads> [dt] [output_csv]\n"
            "  N       : number of bodies\n"
            "  steps   : simulation steps\n"
            "  threads : OpenMP thread count\n"
            "  dt      : time step (default 0.001)\n"
        );
        return 1;
    }

    int    N       = std::atoi(argv[1]);
    int    steps   = std::atoi(argv[2]);
    int    threads = std::atoi(argv[3]);
    double dt      = (argc >= 5) ? std::atof(argv[4]) : pdc::DEFAULT_DT;
    const char* csv_out = (argc >= 6) ? argv[5] : nullptr;

    if (N <= 0 || steps <= 0 || threads <= 0) {
        std::fprintf(stderr, "Error: N, steps and threads must be positive.\n");
        return 1;
    }

#ifdef _OPENMP
    omp_set_num_threads(threads);
    std::printf("[OpenMP N-Body] N=%d  steps=%d  threads=%d  dt=%.4f\n",
                N, steps, threads, dt);
#else
    std::printf("[OpenMP N-Body — OpenMP unavailable, running serial fallback]\n");
    std::printf("N=%d  steps=%d  dt=%.4f\n", N, steps, dt);
#endif

    pdc::Body* bodies = pdc::allocBodies(N);
    pdc::initRandomBodies(bodies, N, 42u);

    double energy0 = pdc::computeTotalEnergy(bodies, N);
    std::printf("Initial total energy: %.6e J\n", energy0);

    auto t_start = pdc::now();

    for (int s = 0; s < steps; ++s) {
        /* computeForceOpenMP directly sets bodies[i].fx/fy/fz (no resetForces needed) */
        computeForceOpenMP(bodies, N);
        pdc::integrateStep(bodies, N, dt);

        if ((s + 1) % 50 == 0 || (s + 1) == steps) {
            std::printf("  Step %4d / %4d\r", s + 1, steps);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    double elapsed = pdc::elapsedMs(t_start);
    double energyF = pdc::computeTotalEnergy(bodies, N);

    pdc::printResult("OpenMP", N, steps, threads, elapsed, energy0, energyF);
    std::printf("ElapsedMs:%.3f\n", elapsed);

    if (csv_out) {
        pdc::writeBodiesCSV(bodies, N, csv_out);
        std::printf("Final positions written to: %s\n", csv_out);
    }

    pdc::freeBodies(bodies);
    return 0;
}
