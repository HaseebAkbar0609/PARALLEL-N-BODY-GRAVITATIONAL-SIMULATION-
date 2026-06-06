#pragma once
/*
 * nbody.hpp — Core data structures and function declarations for the
 *             Parallel N-Body Gravitational Simulation project.
 *
 * PDC Project — Parallel N-Body Simulation
 * Groups: 3 members
 * Covers: MPI, OpenMP, POSIX Threads, Performance Analysis
 */

#include <cstddef>
#include <cstdint>

namespace pdc {

/* ─────────────────────────────────────────────
 * Physical constants used in the simulation
 * ───────────────────────────────────────────── */
constexpr double GRAVITY    = 6.674e-11;   /* gravitational constant G  */
constexpr double SOFTENING  = 1.0e-9;      /* softening length (avoids singularity) */
constexpr double DEFAULT_DT = 0.001;       /* default time-step (seconds)           */

/* ─────────────────────────────────────────────
 * Body — represents a single gravitational body
 * ───────────────────────────────────────────── */
struct Body {
    double x,  y,  z;    /* position components  (m)      */
    double vx, vy, vz;   /* velocity components  (m/s)    */
    double fx, fy, fz;   /* accumulated force    (N)      */
    double mass;          /* mass                 (kg)     */
};

/* ─────────────────────────────────────────────
 * Lifecycle / initialisation
 * ───────────────────────────────────────────── */
/* Allocate an array of n bodies (caller must call freeBodies). */
Body* allocBodies(int n);

/* Free a body array allocated by allocBodies. */
void freeBodies(Body* bodies);

/* Fill the array with randomised positions/velocities/masses. */
void initRandomBodies(Body* bodies, int n, unsigned int seed = 42);

/* Copy all fields from src to dst (both must have length n). */
void copyBodies(Body* dst, const Body* src, int n);

/* ─────────────────────────────────────────────
 * Per-step operations
 * ───────────────────────────────────────────── */
/* Zero out fx/fy/fz for every body. */
void resetForces(Body* bodies, int n);

/*
 * O(n²) all-pairs force computation (serial reference version).
 * Uses Newton's 3rd law: computes the force between every pair (i,j)
 * once and accumulates both directions.
 */
void computeForceSerial(Body* bodies, int n);

/*
 * Leapfrog velocity-Verlet integration for one time step.
 * Updates velocities and positions in-place.
 */
void integrateStep(Body* bodies, int n, double dt);

/* ─────────────────────────────────────────────
 * Diagnostics / correctness checking
 * ───────────────────────────────────────────── */
/*
 * Compute total mechanical energy (kinetic + potential).
 * Used to verify that energy is conserved across implementations.
 * Returns E in Joules (double precision).
 */
double computeTotalEnergy(const Body* bodies, int n);

/* ─────────────────────────────────────────────
 * Output helpers
 * ───────────────────────────────────────────── */
/* Print a formatted performance + correctness summary line. */
void printResult(
    const char* label,
    int   n,
    int   steps,
    int   parallel_units,
    double elapsed_ms,
    double energy_initial,
    double energy_final
);

/* Write final body positions to a CSV file (for optional visualisation). */
void writeBodiesCSV(const Body* bodies, int n, const char* filename);

} // namespace pdc
