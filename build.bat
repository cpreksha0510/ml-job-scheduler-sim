@echo off
REM ============================================================================
REM  build.bat  --  Build the ML Job Scheduler Simulator (no CMake)
REM
REM  Usage:
REM    build.bat
REM
REM  Produces:  sim.exe and experiments.exe in the project root.
REM
REM  Requirements: g++ on PATH, C++14, STL only.
REM  Exit codes:   0 = success, non-zero = compilation error.
REM ============================================================================

setlocal EnableDelayedExpansion

echo [build] Collecting source files...

REM -- Always-present sources ---------------------------------------------------
set "SRCS=src\simulator.cpp src\metrics.cpp src\workload.cpp src\main.cpp"

REM -- Optional policy sources (directory may be empty at this stage) -----------
REM    We enumerate them explicitly so the glob never reaches g++ unexpanded.
set "POLICY_SRCS="
for %%f in (src\policies\*.cpp) do (
    set "POLICY_SRCS=!POLICY_SRCS! %%f"
)

if not "!POLICY_SRCS!"=="" (
    echo [build] Policy sources found:!POLICY_SRCS!
    set "SRCS=!SRCS!!POLICY_SRCS!"
) else (
    echo [build] No policy .cpp files found -- building core only.
)

REM -- Compile sim.exe ----------------------------------------------------------
echo [build] Compiling sim.exe...
echo         g++ -std=c++14 -Wall -Wextra -Iinclude %SRCS% -o sim

g++ -std=c++14 -Wall -Wextra -Iinclude %SRCS% -o sim

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [build] FAILED compiling sim.exe
    exit /b 1
)

REM -- Compile experiments.exe --------------------------------------------------
echo [build] Compiling experiments.exe...
set "EXP_SRCS=src\simulator.cpp src\metrics.cpp src\workload.cpp src\experiment.cpp!POLICY_SRCS!"
echo         g++ -std=c++14 -Wall -Wextra -Iinclude !EXP_SRCS! -o experiments

g++ -std=c++14 -Wall -Wextra -Iinclude !EXP_SRCS! -o experiments

REM -- Compile benchmark.exe (-O2) --------------------------------------------
echo [build] Compiling benchmark.exe (-O2)...
set "BENCH_SRCS=src\simulator.cpp src\metrics.cpp src\workload.cpp src\benchmark.cpp!POLICY_SRCS!"
echo         g++ -std=c++14 -O2 -Wall -Wextra -Iinclude -Isrc !BENCH_SRCS! -o benchmark

g++ -std=c++14 -O2 -Wall -Wextra -Iinclude -Isrc !BENCH_SRCS! -o benchmark

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [build] FAILED compiling benchmark.exe
    exit /b 1
)

echo [build] SUCCESS  --  sim.exe, experiments.exe, and benchmark.exe are ready.
exit /b 0
