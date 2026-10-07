@echo off
REM ============================================================================
REM  build.bat  --  Build the ML Job Scheduler Simulator (no CMake)
REM
REM  Usage:
REM    build.bat
REM
REM  Produces:  sim.exe  in the project root.
REM
REM  Requirements: g++ on PATH, C++14, STL only.
REM  Exit codes:   0 = success, non-zero = compilation error.
REM ============================================================================

setlocal EnableDelayedExpansion

echo [build] Collecting source files...

REM -- Always-present sources ---------------------------------------------------
set "SRCS=src\simulator.cpp src\metrics.cpp src\main.cpp"

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

REM -- Compile ------------------------------------------------------------------
echo [build] Running g++...
echo         g++ -std=c++14 -Wall -Wextra -Iinclude %SRCS% -o sim

g++ -std=c++14 -Wall -Wextra -Iinclude %SRCS% -o sim

set BUILD_EXIT=%ERRORLEVEL%

if %BUILD_EXIT% NEQ 0 (
    echo.
    echo [build] FAILED  (g++ exited with code %BUILD_EXIT%)
    exit /b %BUILD_EXIT%
)

echo [build] SUCCESS  --  sim.exe is ready.
exit /b 0
