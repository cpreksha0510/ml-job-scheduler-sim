@echo off
REM ============================================================================
REM  test.bat  --  Run integration tests for the ML Job Scheduler Simulator
REM
REM  Usage:
REM    test.bat
REM
REM  Strategy:
REM    1. Build sim.exe via build.bat (fails fast if compilation breaks).
REM    2. Build and run tests\test_fcfs  (FCFS unit tests, plain asserts).
REM    3. Run smoke test: sim.exe against data/sample_jobs.csv.
REM    4. Run edge cases: missing CSV arg, non-existent file.
REM    5. Print PASS/FAIL summary and exit non-zero if any test failed.
REM
REM  Exit codes:  0 = all tests passed, 1 = one or more tests failed.
REM ============================================================================

setlocal EnableDelayedExpansion

set PASS=0
set FAIL=0

REM ============================================================================
REM  Helper: record result
REM    call :check_result <expected_exit> <actual_exit> <test_name>
REM ============================================================================
goto :main

:check_result
    set "EXPECTED=%~1"
    set "ACTUAL=%~2"
    set "TNAME=%~3"
    if "%ACTUAL%"=="%EXPECTED%" (
        echo [PASS] %TNAME%
        set /a PASS+=1
    ) else (
        echo [FAIL] %TNAME%  ^(expected exit %EXPECTED%, got %ACTUAL%^)
        set /a FAIL+=1
    )
    goto :eof

REM ============================================================================
REM  main
REM ============================================================================
:main

echo ============================================================
echo  ML Job Scheduler Simulator -- Test Suite
echo ============================================================
echo.

REM ----------------------------------------------------------------------------
REM  Step 1: Build sim.exe
REM ----------------------------------------------------------------------------
echo --- Step 1: Build sim.exe ---
call build.bat
set BUILD_RC=%ERRORLEVEL%
if %BUILD_RC% NEQ 0 (
    echo.
    echo [FATAL] Build failed. Aborting tests.
    exit /b 1
)
echo.

REM ----------------------------------------------------------------------------
REM  Step 2: Build and run FCFS unit tests
REM    Compiles its own binary (test_fcfs.exe) -- separate from sim.exe.
REM    The binary prints [PASS]/[FAIL] per assertion and exits 0 on success.
REM ----------------------------------------------------------------------------
echo --- Step 2: FCFS unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\policies\fcfs.cpp tests\test_fcfs.cpp -o test_fcfs
if %ERRORLEVEL% NEQ 0 goto :fcfs_compile_fail

test_fcfs.exe
if %ERRORLEVEL% EQU 0 goto :fcfs_pass

echo [FAIL] test_fcfs returned a non-zero exit code.
set /a FAIL+=1
goto :fcfs_done

:fcfs_compile_fail
echo [FAIL] test_fcfs.cpp failed to compile.
set /a FAIL+=1
goto :fcfs_done

:fcfs_pass
set /a PASS+=1

:fcfs_done
echo.

REM ----------------------------------------------------------------------------
REM  Test 1: Smoke test with sample CSV (expect exit 0)
REM ----------------------------------------------------------------------------
echo --- Test 1: Smoke test (sample_jobs.csv) ---
sim.exe data\sample_jobs.csv > nul 2>&1
call :check_result 0 %ERRORLEVEL% "Smoke test: sim runs on sample_jobs.csv"
echo.

REM ----------------------------------------------------------------------------
REM  Test 2: Smoke test with explicit max_ticks argument (expect exit 0)
REM ----------------------------------------------------------------------------
echo --- Test 2: Smoke test with max_ticks=50 ---
sim.exe data\sample_jobs.csv 50 > nul 2>&1
call :check_result 0 %ERRORLEVEL% "Smoke test: sim runs with max_ticks=50"
echo.

REM ----------------------------------------------------------------------------
REM  Test 3: No arguments -- should exit non-zero (usage error)
REM ----------------------------------------------------------------------------
echo --- Test 3: No arguments (expect non-zero) ---
sim.exe > nul 2>&1
set RC3=%ERRORLEVEL%
if %RC3% NEQ 0 (
    call :check_result %RC3% %RC3% "No-arg invocation returns non-zero"
) else (
    echo [FAIL] No-arg invocation returned 0 (expected non-zero^)
    set /a FAIL+=1
)
echo.

REM ----------------------------------------------------------------------------
REM  Test 4: Non-existent CSV -- should exit non-zero
REM ----------------------------------------------------------------------------
echo --- Test 4: Non-existent file (expect non-zero) ---
sim.exe data\does_not_exist.csv > nul 2>&1
set RC4=%ERRORLEVEL%
if %RC4% NEQ 0 (
    call :check_result %RC4% %RC4% "Non-existent CSV returns non-zero"
) else (
    echo [FAIL] Non-existent CSV returned 0 (expected non-zero^)
    set /a FAIL+=1
)
echo.

REM ----------------------------------------------------------------------------
REM  Summary
REM ----------------------------------------------------------------------------
echo ============================================================
echo  Results: %PASS% passed, %FAIL% failed
echo ============================================================

if %FAIL% NEQ 0 (
    exit /b 1
)
exit /b 0
