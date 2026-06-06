/*
 * serial_nbody.cpp — Serial (single-threaded) N-Body Gravitational Simulation.
 *
 * This is the BASELINE implementation used for all speedup calculations.
 * It runs entirely on one CPU core with no parallelism.
 *
 * Algorithm:
 *   For each time step:
 *     1. Reset accumulated forces to zero   O(n)
 *     2. Compute all-pairs gravitational forces  O(n²)  ← dominant cost
 *     3. Integrate velocities and positions  O(n)
 *
 * Total work:  O(n² × steps)
 * Memory:      O(n) — one Body array
 *
 * Usage:  serial_nbody  <N>  <steps>  [dt]  [output_csv]
 *   N          : number of bodies
 *   steps      : number of simulation steps
 *   dt         : time step in seconds (default: 0.001)
 *   output_csv : optional path to write final body positions
 *
 * Example:
 *   serial_nbody 512 200
 *   serial_nbody 1024 100 0.001 results/serial_final.csv
 */

#include "nbody.hpp"
#include "timer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    /* ── Argument parsing ─────────────────────────────────────────── */
    if (argc < 3) {
        std::fprintf(stderr,
            "Usage: serial_nbody <N> <steps> [dt] [output_csv]\n"
            "  N        : number of bodies\n"
            "  steps    : number of simulation steps\n"
            "  dt       : time step in seconds (default 0.001)\n"
            "  output_csv : optional CSV output path\n"
        );
        return 1;
    }

    int    N     = std::atoi(argv[1]);
    int    steps = std::atoi(argv[2]);
    double dt    = (argc >= 4) ? std::atof(argv[3]) : pdc::DEFAULT_DT;
    const char* csv_out = (argc >= 5) ? argv[4] : nullptr;

    if (N <= 0 || steps <= 0) {
        std::fprintf(stderr, "Error: N and steps must be positive integers.\n");
        return 1;
    }

    /* ── Initialise ───────────────────────────────────────────────── */
    pdc::Body* bodies = pdc::allocBodies(N);
    pdc::initRandomBodies(bodies, N, 42u);

    double energy0 = pdc::computeTotalEnergy(bodies, N);

    std::printf("[Serial N-Body] N=%d  steps=%d  dt=%.4f\n", N, steps, dt);
    std::printf("Initial total energy: %.6e J\n", energy0);

    /* ── Simulation loop ──────────────────────────────────────────── */
    auto t_start = pdc::now();

    for (int s = 0; s < steps; ++s) {
        pdc::resetForces(bodies, N);
        pdc::computeForceSerial(bodies, N);
        pdc::integrateStep(bodies, N, dt);

        /* Print progress every 50 steps so the user can see the demo is alive */
        if ((s + 1) % 50 == 0 || (s + 1) == steps) {
            std::printf("  Step %4d / %4d completed\r", s + 1, steps);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    double elapsed = pdc::elapsedMs(t_start);
    double energyF = pdc::computeTotalEnergy(bodies, N);

    /* ── Results ──────────────────────────────────────────────────── */
    pdc::printResult("Serial", N, steps, 1, elapsed, energy0, energyF);
    std::printf("ElapsedMs:%.3f\n", elapsed);  /* machine-parseable for benchmark script */

    /* Optional CSV output */
    if (csv_out) {
        pdc::writeBodiesCSV(bodies, N, csv_out);
        std::printf("Final positions written to: %s\n", csv_out);
    }

    pdc::freeBodies(bodies);
    return 0;
}
