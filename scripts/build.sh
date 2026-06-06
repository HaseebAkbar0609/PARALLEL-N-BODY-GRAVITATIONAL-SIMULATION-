#!/usr/bin/env bash
# =============================================================
# build.sh — One-shot build script for MSYS2 MinGW64
#
# Run from the project root inside the MSYS2 MinGW64 shell:
#   cd "/c/Users/Dr.Tech/OneDrive/Desktop/PDC Lab/pdc-parallel-nbody"
#   bash scripts/build.sh
# =============================================================
set -euo pipefail

# cd into project root so all paths below are relative (no spaces issue)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

GXX="/mingw64/bin/g++"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"
INC="-Isrc/common"
COMMON="src/common/nbody.cpp"

mkdir -p build results

echo "========================================"
echo " PDC N-Body — Build Script"
echo " Working dir: $(pwd)"
echo "========================================"

# ── 1. Serial ──────────────────────────────────────────────
echo ""
echo "[1/4] Building serial_nbody..."
$GXX $CXXFLAGS $INC \
    $COMMON src/apps/serial_nbody.cpp \
    -o build/serial_nbody.exe
echo "      -> build/serial_nbody.exe  [DONE]"

# ── 2. OpenMP ──────────────────────────────────────────────
echo ""
echo "[2/4] Building openmp_nbody..."
$GXX $CXXFLAGS -fopenmp $INC \
    $COMMON src/apps/openmp_nbody.cpp \
    -o build/openmp_nbody.exe
echo "      -> build/openmp_nbody.exe  [DONE]"

# ── 3. Pthreads ────────────────────────────────────────────
echo ""
echo "[3/4] Building pthread_nbody..."
$GXX $CXXFLAGS $INC \
    $COMMON src/apps/pthread_nbody.cpp \
    -lpthread -o build/pthread_nbody.exe
echo "      -> build/pthread_nbody.exe  [DONE]"

# ── 4. MPI + OpenMP ────────────────────────────────────────
echo ""
echo "[4/4] Building mpi_nbody (MPI+OpenMP)..."
if [ -f "/mingw64/include/mpi.h" ]; then
    $GXX $CXXFLAGS -fopenmp \
        -I/mingw64/include $INC \
        $COMMON src/apps/mpi_nbody.cpp \
        -L/mingw64/lib -lmsmpi -lpthread \
        -o build/mpi_nbody.exe
    echo "      -> build/mpi_nbody.exe  [DONE]"
else
    echo "      [WARN] mpi.h not found. Install: pacman -S mingw-w64-x86_64-msmpi"
fi

echo ""
echo "========================================"
echo " All builds complete!"
echo "========================================"
echo ""
echo " Smoke tests:"
echo "   ./build/serial_nbody.exe 256 50"
echo "   ./build/openmp_nbody.exe 256 50 4"
echo "   ./build/pthread_nbody.exe 256 50 4"
echo "   mpiexec -n 2 ./build/mpi_nbody.exe 256 50 2"
echo ""
echo " Full benchmark:"
echo "   python scripts/run_benchmarks.py --build-dir build --out results/benchmark.csv"
