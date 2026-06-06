$env:PATH = "C:\msys64\mingw64\bin;C:\Program Files\Microsoft MPI\Bin;" + $env:PATH
Set-Location "C:\Users\Dr.Tech\OneDrive\Desktop\PDC Lab\pdc-parallel-nbody"
$m = "C:\Program Files\Microsoft MPI\Bin\mpiexec.exe"
$e = ".\build\mpi_nbody.exe"

Write-Host "=== MPI+OpenMP N-Body Benchmarks ==="
Write-Host "--- 1 rank, 1 OMP thread, N=1024, 50 steps ---"
& $m -n 1 $e 1024 50 1
Write-Host "--- 2 ranks, 1 OMP thread, N=1024, 50 steps ---"
& $m -n 2 $e 1024 50 1
Write-Host "--- 2 ranks, 2 OMP threads, N=1024, 50 steps ---"
& $m -n 2 $e 1024 50 2
Write-Host "--- 4 ranks, 1 OMP thread, N=1024, 50 steps ---"
& $m -n 4 $e 1024 50 1
Write-Host "--- 4 ranks, 2 OMP threads, N=4096, 10 steps ---"
& $m -n 4 $e 4096 10 2
Write-Host "=== DONE ==="
