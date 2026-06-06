/*
 * pthread_nbody.cpp — POSIX Threads N-Body Gravitational Simulation.
 *
 * PDC Concepts Demonstrated
 * ─────────────────────────
 *   • pthread_create / pthread_join    — thread lifecycle management
 *   • pthread_barrier_wait             — barrier synchronisation between steps
 *   • Lock-free parallel computation   — static striped row assignment guarantees
 *                                         no two threads ever write the same body,
 *                                         so NO mutex is needed in the hot path.
 *   • Persistent thread pool           — threads live for all steps, avoiding
 *                                         pthread_create overhead per step.
 *   • Workload decomposition           — rows assigned as i % T == tid
 *
 * Design: 2-Barrier Work-Sharing
 * ─────────────────────────────────
 *   Threads are created ONCE before the simulation loop and use two barriers
 *   per step:
 *
 *   Phase 1 — Force compute (all workers in parallel, write to bodies[i].fx)
 *              barrier_after_compute  <- main + all workers rendezvous
 *   Phase 2 — Integration (main thread only, O(n))
 *              barrier_after_integrate <- signals workers to start next step
 *
 *   Because each worker owns a unique disjoint subset of rows (striped), no
 *   two threads ever write to the same memory address during force computation.
 *   The mutex seen in many textbook implementations is unnecessary and was the
 *   primary cause of poor scaling -- removing it gives near-linear speedup.
 *
 * Deadlock Prevention
 * ────────────────────
 *   Barriers are reached in identical order by every thread (workers + main).
 *   No thread ever holds a lock, so deadlock is structurally impossible.
 *
 * Race Condition Prevention
 * ──────────────────────────
 *   • Striped row assignment: thread tid owns rows i where i % T == tid.
 *     No overlap => no write-write race on bodies[i].fx/fy/fz.
 *   • bodies[j] is read-only during force computation -- safe for all threads.
 *   • Integration (writes .vx, .x etc.) runs after barrier; all force writes
 *     are visible by the time main integrates.
 *
 * Usage:  pthread_nbody  <N>  <steps>  <threads>  [dt]  [output_csv]
 */

#include "nbody.hpp"
#include "timer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <pthread.h>

/* -------------------------------------------------------------------------
 * Shared state visible to all worker threads
 * ------------------------------------------------------------------------- */
struct SharedState {
    pdc::Body*   bodies;       /* shared body array                       */
    int          n;            /* total number of bodies                  */
    int          num_threads;  /* total worker count                      */
    double       dt;           /* integration time step                   */
    int          steps;        /* total simulation steps                  */
    volatile int done;         /* set to 1 by main to stop threads        */

    pthread_barrier_t barrier_after_compute;   /* after force compute     */
    pthread_barrier_t barrier_after_integrate; /* after integration       */
};

/* -------------------------------------------------------------------------
 * Worker thread function
 * ------------------------------------------------------------------------- */
struct WorkerArg {
    SharedState* state;
    int          tid;   /* thread id: 0 .. num_threads-1 */
};

static void* workerFunc(void* arg) {
    WorkerArg*   wa  = static_cast<WorkerArg*>(arg);
    SharedState* s   = wa->state;
    int          tid = wa->tid;
    int          n   = s->n;
    int          T   = s->num_threads;

    /*
     * NOTE: done check is placed AFTER both barriers, not before Phase 1.
     * This eliminates a race where main sets done=1 between barrier_after_integrate
     * and the while-check, causing workers to exit before the shutdown barriers
     * and leaving main deadlocked. Workers always go through both barriers before
     * inspecting done, so the shutdown sequence in main is always safe.
     */
    while (true) {
        /* Phase 1: Force computation (lock-free, skipped on shutdown) */
        /*
         * Thread tid owns rows where i % T == tid (striped assignment).
         * It reads from all j (read-only, safe) and writes ONLY to its
         * own rows (no other thread writes to the same index).
         * Therefore no synchronisation primitive is needed here.
         */
        if (!s->done) {
            for (int i = tid; i < n; i += T) {
                double acc_fx = 0.0, acc_fy = 0.0, acc_fz = 0.0;
                for (int j = 0; j < n; ++j) {
                    if (i == j) continue;

                    double dx = s->bodies[j].x - s->bodies[i].x;
                    double dy = s->bodies[j].y - s->bodies[i].y;
                    double dz = s->bodies[j].z - s->bodies[i].z;

                    double distSq  = dx*dx + dy*dy + dz*dz
                                   + pdc::SOFTENING * pdc::SOFTENING;
                    double dist    = sqrt(distSq);
                    double distCub = distSq * dist;

                    double F = pdc::GRAVITY
                             * s->bodies[i].mass * s->bodies[j].mass / distCub;

                    acc_fx += F * dx;
                    acc_fy += F * dy;
                    acc_fz += F * dz;
                }
                /* Direct write -- safe: only this thread owns index i */
                s->bodies[i].fx = acc_fx;
                s->bodies[i].fy = acc_fy;
                s->bodies[i].fz = acc_fz;
            }
        }

        /* Rendezvous: all workers + main thread */
        pthread_barrier_wait(&s->barrier_after_compute);

        /* Shutdown check: main sets done=1 then calls barrier_compute.
         * Workers exit HERE — before barrier_integrate — so main must NOT
         * call barrier_integrate during shutdown.  This is race-free because
         * done is always read after a barrier (full memory fence). */
        if (s->done) break;

        /* Main is integrating.  Workers wait here until it finishes. */
        pthread_barrier_wait(&s->barrier_after_integrate);
    }

    return nullptr;
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "Usage: pthread_nbody <N> <steps> <threads> [dt] [output_csv]\n"
            "  N       : number of bodies\n"
            "  steps   : simulation steps\n"
            "  threads : POSIX thread count\n"
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
        std::fprintf(stderr, "Error: N, steps, threads must be positive.\n");
        return 1;
    }

    std::printf("[Pthreads N-Body] N=%d  steps=%d  threads=%d  dt=%.4f\n",
                N, steps, threads, dt);

    /* Allocate and initialise */
    pdc::Body* bodies = pdc::allocBodies(N);
    pdc::initRandomBodies(bodies, N, 42u);
    double energy0 = pdc::computeTotalEnergy(bodies, N);
    std::printf("Initial total energy: %.6e J\n", energy0);

    /* Initialise SharedState */
    SharedState state{};
    state.bodies      = bodies;
    state.n           = N;
    state.num_threads = threads;
    state.dt          = dt;
    state.steps       = steps;
    state.done        = 0;

    /* threads + 1 (main) must reach each barrier */
    unsigned bar_count = static_cast<unsigned>(threads + 1);
    pthread_barrier_init(&state.barrier_after_compute,   nullptr, bar_count);
    pthread_barrier_init(&state.barrier_after_integrate, nullptr, bar_count);

    /* Spawn worker threads */
    std::vector<pthread_t>  thread_ids(static_cast<std::size_t>(threads));
    std::vector<WorkerArg>  worker_args(static_cast<std::size_t>(threads));
    for (int t = 0; t < threads; ++t) {
        worker_args[static_cast<std::size_t>(t)] = { &state, t };
        pthread_create(
            &thread_ids[static_cast<std::size_t>(t)],
            nullptr, workerFunc,
            &worker_args[static_cast<std::size_t>(t)]
        );
    }

    /* Simulation loop (main thread coordinates) */
    auto t_start = pdc::now();

    for (int s = 0; s < steps; ++s) {
        /* Workers are computing forces while main waits at barrier */
        pthread_barrier_wait(&state.barrier_after_compute);

        /* All force writes are complete -- safe to integrate */
        pdc::integrateStep(bodies, N, dt);

        /* Release workers to start the next step */
        pthread_barrier_wait(&state.barrier_after_integrate);

        if ((s + 1) % 50 == 0 || (s + 1) == steps) {
            std::printf("  Step %4d / %4d\r", s + 1, steps);
            std::fflush(stdout);
        }
    }
    std::printf("\n");

    double elapsed = pdc::elapsedMs(t_start);

    /* Shut down workers:
     * Set done=1, then call barrier_compute so workers can see it and exit.
     * Do NOT call barrier_integrate — workers break before reaching it. */
    state.done = 1;
    pthread_barrier_wait(&state.barrier_after_compute);
    for (int t = 0; t < threads; ++t) {
        pthread_join(thread_ids[static_cast<std::size_t>(t)], nullptr);
    }

    pthread_barrier_destroy(&state.barrier_after_compute);
    pthread_barrier_destroy(&state.barrier_after_integrate);

    /* Results */
    double energyF = pdc::computeTotalEnergy(bodies, N);
    pdc::printResult("Pthreads", N, steps, threads, elapsed, energy0, energyF);
    std::printf("ElapsedMs:%.3f\n", elapsed);

    if (csv_out) {
        pdc::writeBodiesCSV(bodies, N, csv_out);
        std::printf("Final positions written to: %s\n", csv_out);
    }

    pdc::freeBodies(bodies);
    return 0;
}