# ============================================================
# Makefile — PDC Parallel N-Body Gravitational Simulation
# ============================================================
# Works inside MSYS2 MinGW64 shell on Windows.
# Tested with: GCC 16 / OpenMP 5 / MSMPI
#
# Usage (from MSYS2 MinGW64 prompt):
#   make              — build all targets
#   make serial       — build only serial_nbody
#   make openmp       — build only openmp_nbody
#   make pthreads     — build only pthread_nbody
#   make mpi          — build only mpi_nbody  (needs msmpi)
#   make clean        — remove all build artefacts
#   make run_serial   — quick smoke-test (N=256, 50 steps)
#   make run_all      — run all four smoke tests
#   make benchmark    — run full benchmark suite (needs Python 3)
# ============================================================

# ── Compiler settings ───────────────────────────────────────
CXX      := g++
MPICXX   := g++       # MSMPI uses plain g++ with manual -I/-L flags
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic

# Include dirs
COMMON_INC := -Isrc/common

# ── OpenMP flag ─────────────────────────────────────────────
OMP_FLAG := -fopenmp

# ── Pthreads flag ───────────────────────────────────────────
PTHREAD_FLAGS := -lpthread

# ── MSMPI flags (MSYS2 mingw-w64-x86_64-msmpi) ─────────────
# Headers land directly in /mingw64/include/ (no msmpi subdir)
MSMPI_INC := -I/mingw64/include
MSMPI_LIB := -L/mingw64/lib -lmsmpi

# ── Output directory ────────────────────────────────────────
BUILD_DIR := build

# ── Source files ────────────────────────────────────────────
COMMON_SRC := src/common/nbody.cpp

# ============================================================
# Targets
# ============================================================
.PHONY: all serial openmp pthreads mpi clean \
        run_serial run_openmp run_pthreads run_mpi run_all benchmark

all: serial openmp pthreads mpi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Serial ──────────────────────────────────────────────────
serial: $(BUILD_DIR) $(BUILD_DIR)/serial_nbody.exe

$(BUILD_DIR)/serial_nbody.exe: $(COMMON_SRC) src/apps/serial_nbody.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_INC) $^ -o $@
	@echo "[OK] serial_nbody built"

# ── OpenMP ──────────────────────────────────────────────────
openmp: $(BUILD_DIR) $(BUILD_DIR)/openmp_nbody.exe

$(BUILD_DIR)/openmp_nbody.exe: $(COMMON_SRC) src/apps/openmp_nbody.cpp
	$(CXX) $(CXXFLAGS) $(OMP_FLAG) $(COMMON_INC) $^ -o $@
	@echo "[OK] openmp_nbody built"

# ── Pthreads ────────────────────────────────────────────────
pthreads: $(BUILD_DIR) $(BUILD_DIR)/pthread_nbody.exe

$(BUILD_DIR)/pthread_nbody.exe: $(COMMON_SRC) src/apps/pthread_nbody.cpp
	$(CXX) $(CXXFLAGS) $(COMMON_INC) $^ $(PTHREAD_FLAGS) -o $@
	@echo "[OK] pthread_nbody built"

# ── MPI + OpenMP ────────────────────────────────────────────
mpi: $(BUILD_DIR) $(BUILD_DIR)/mpi_nbody.exe

$(BUILD_DIR)/mpi_nbody.exe: $(COMMON_SRC) src/apps/mpi_nbody.cpp
	$(MPICXX) $(CXXFLAGS) $(OMP_FLAG) $(MSMPI_INC) $(COMMON_INC) $^ \
	    $(MSMPI_LIB) $(PTHREAD_FLAGS) -o $@
	@echo "[OK] mpi_nbody built"

# ── Clean ───────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR)
	@echo "[OK] build directory removed"

# ============================================================
# Smoke-test runs
# ============================================================
N_DEMO  := 256
S_DEMO  := 50

run_serial: serial
	@echo "--- Serial smoke test ---"
	$(BUILD_DIR)/serial_nbody.exe $(N_DEMO) $(S_DEMO)

run_openmp: openmp
	@echo "--- OpenMP smoke test (4 threads) ---"
	$(BUILD_DIR)/openmp_nbody.exe $(N_DEMO) $(S_DEMO) 4

run_pthreads: pthreads
	@echo "--- Pthreads smoke test (4 threads) ---"
	$(BUILD_DIR)/pthread_nbody.exe $(N_DEMO) $(S_DEMO) 4

run_mpi: mpi
	@echo "--- MPI+OpenMP smoke test (2 ranks x 2 OMP threads) ---"
	mpiexec -n 2 $(BUILD_DIR)/mpi_nbody.exe $(N_DEMO) $(S_DEMO) 2

run_all: run_serial run_openmp run_pthreads run_mpi

# ============================================================
# Full benchmark (requires Python 3 on PATH)
# ============================================================
benchmark: all
	@echo "--- Running full benchmark suite ---"
	python scripts/run_benchmarks.py \
	    --build-dir $(BUILD_DIR) \
	    --out results/benchmark.csv
	@echo "--- Plotting results ---"
	python scripts/plot_results.py \
	    --csv results/benchmark.csv \
	    --out-dir results/
	@echo "Benchmark complete. See results/ folder."
