/*
 * nbody.cpp — Implementation of the core N-Body simulation library.
 *
 * All functions here are serial, single-threaded reference implementations.
 * The parallel front-ends (openmp_nbody, pthread_nbody, mpi_nbody) use these
 * helpers for initialisation, integration, and diagnostics; they parallelise
 * only the computationally dominant force-calculation and integration loops.
 *
 * Physics model
 * ─────────────
 *   Force on body i due to body j:
 *
 *       F_ij = G * m_i * m_j / (r_ij² + ε²)
 *
 *   where ε (SOFTENING) prevents division by zero when two bodies are
 *   very close together.
 *
 *   Integration: Euler method (first-order, sufficient for speedup demos).
 *       v_i(t+dt) = v_i(t) + (F_i / m_i) * dt
 *       x_i(t+dt) = x_i(t) + v_i(t+dt) * dt
 */

#include "nbody.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <new>

namespace pdc {

/* ────────────────────────────────────────────────────────────────────────
 * Memory management
 * ──────────────────────────────────────────────────────────────────────── */

Body* allocBodies(int n) {
    assert(n > 0);
    return new Body[static_cast<std::size_t>(n)];
}

void freeBodies(Body* bodies) {
    delete[] bodies;
}

/* ────────────────────────────────────────────────────────────────────────
 * Initialisation
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * initRandomBodies — Fills bodies with reproducible pseudo-random values.
 *
 * Positions:  uniformly distributed in [-1e10, 1e10] metres   (≈ solar-system scale)
 * Velocities: uniformly distributed in [-1e3,  1e3]  m/s      (modest orbital speeds)
 * Masses:     uniformly distributed in [1e20,  1e30] kg        (asteroid to star range)
 * Forces:     zeroed (accumulated during simulation steps)
 */
void initRandomBodies(Body* bodies, int n, unsigned int seed) {
    /* Use a simple Linear Congruential Generator so we get identical
       initial conditions across all implementations (serial, OpenMP,
       pthreads, MPI) — essential for correct performance comparison. */
    unsigned int state = seed;
    auto lcg = [&]() -> double {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state) / static_cast<double>(0xFFFFFFFFu);
    };

    constexpr double POS_RANGE  = 1.0e10;
    constexpr double VEL_RANGE  = 1.0e3;
    constexpr double MASS_MIN   = 1.0e20;
    constexpr double MASS_MAX   = 1.0e30;

    for (int i = 0; i < n; ++i) {
        bodies[i].x    = (lcg() * 2.0 - 1.0) * POS_RANGE;
        bodies[i].y    = (lcg() * 2.0 - 1.0) * POS_RANGE;
        bodies[i].z    = (lcg() * 2.0 - 1.0) * POS_RANGE;

        bodies[i].vx   = (lcg() * 2.0 - 1.0) * VEL_RANGE;
        bodies[i].vy   = (lcg() * 2.0 - 1.0) * VEL_RANGE;
        bodies[i].vz   = (lcg() * 2.0 - 1.0) * VEL_RANGE;

        bodies[i].mass = MASS_MIN + lcg() * (MASS_MAX - MASS_MIN);

        bodies[i].fx   = 0.0;
        bodies[i].fy   = 0.0;
        bodies[i].fz   = 0.0;
    }
}

void copyBodies(Body* dst, const Body* src, int n) {
    std::memcpy(dst, src, static_cast<std::size_t>(n) * sizeof(Body));
}

/* ────────────────────────────────────────────────────────────────────────
 * Per-step operations
 * ──────────────────────────────────────────────────────────────────────── */

void resetForces(Body* bodies, int n) {
    for (int i = 0; i < n; ++i) {
        bodies[i].fx = 0.0;
        bodies[i].fy = 0.0;
        bodies[i].fz = 0.0;
    }
}

/*
 * computeForceSerial — Reference O(n²) all-pairs force kernel.
 *
 * For each ordered pair (i,j) with i < j we compute the gravitational
 * force magnitude and apply it to BOTH bodies (Newton's 3rd law) so that
 * we only iterate over n*(n-1)/2 pairs instead of n² — roughly a 2x
 * constant-factor speedup in the serial baseline, which makes the
 * parallel speedup comparisons even cleaner.
 */
void computeForceSerial(Body* bodies, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;

            double distSq  = dx*dx + dy*dy + dz*dz + SOFTENING*SOFTENING;
            double dist    = std::sqrt(distSq);
            double distCub = distSq * dist;

            double F = GRAVITY * bodies[i].mass * bodies[j].mass / distCub;

            bodies[i].fx += F * dx;
            bodies[i].fy += F * dy;
            bodies[i].fz += F * dz;

            /* Reaction force on j (Newton's 3rd law) */
            bodies[j].fx -= F * dx;
            bodies[j].fy -= F * dy;
            bodies[j].fz -= F * dz;
        }
    }
}

/*
 * integrateStep — Euler integration.
 *
 * Precondition: resetForces + computeForce* have been called this step.
 * Postcondition: velocities and positions are updated; forces remain set
 *                (caller should call resetForces before the next step).
 */
void integrateStep(Body* bodies, int n, double dt) {
    for (int i = 0; i < n; ++i) {
        double inv_mass = 1.0 / bodies[i].mass;

        bodies[i].vx += bodies[i].fx * inv_mass * dt;
        bodies[i].vy += bodies[i].fy * inv_mass * dt;
        bodies[i].vz += bodies[i].fz * inv_mass * dt;

        bodies[i].x  += bodies[i].vx * dt;
        bodies[i].y  += bodies[i].vy * dt;
        bodies[i].z  += bodies[i].vz * dt;
    }
}

/* ────────────────────────────────────────────────────────────────────────
 * Diagnostics
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * computeTotalEnergy — O(n²) total mechanical energy of the system.
 *
 * E_total = KE + PE
 *   KE = ½ * m * v²
 *   PE = –G * m_i * m_j / r_ij   (summed over all pairs)
 *
 * Used to verify energy conservation across different implementations.
 * A significant difference (> 1e-6 relative) indicates a bug.
 */
double computeTotalEnergy(const Body* bodies, int n) {
    double KE = 0.0;
    double PE = 0.0;

    for (int i = 0; i < n; ++i) {
        double v2 = bodies[i].vx * bodies[i].vx
                  + bodies[i].vy * bodies[i].vy
                  + bodies[i].vz * bodies[i].vz;
        KE += 0.5 * bodies[i].mass * v2;
    }

    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz + SOFTENING*SOFTENING);
            PE -= GRAVITY * bodies[i].mass * bodies[j].mass / dist;
        }
    }

    return KE + PE;
}

/* ────────────────────────────────────────────────────────────────────────
 * Output helpers
 * ──────────────────────────────────────────────────────────────────────── */

void printResult(
    const char* label,
    int    n,
    int    steps,
    int    parallel_units,
    double elapsed_ms,
    double energy_initial,
    double energy_final
) {
    double rel_energy_err = std::abs((energy_final - energy_initial) / energy_initial);
    std::printf(
        "%-30s  N=%6d  steps=%5d  P=%2d  Time=%10.3f ms  "
        "EnergyError=%.6e  %s\n",
        label, n, steps, parallel_units, elapsed_ms,
        rel_energy_err,
        (rel_energy_err < 1.0e-4 ? "[PASS]" : "[DRIFT]")
    );
    std::fflush(stdout);
}

void writeBodiesCSV(const Body* bodies, int n, const char* filename) {
    FILE* fp = std::fopen(filename, "w");
    if (!fp) return;
    std::fprintf(fp, "id,x,y,z,vx,vy,vz,mass\n");
    for (int i = 0; i < n; ++i) {
        std::fprintf(fp, "%d,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e\n",
            i,
            bodies[i].x, bodies[i].y, bodies[i].z,
            bodies[i].vx, bodies[i].vy, bodies[i].vz,
            bodies[i].mass);
    }
    std::fclose(fp);
}

} // namespace pdc
