@echo off
REM ============================================================================
REM  test.bat  --  Run integration tests for the ML Job Scheduler Simulator
REM
REM  Usage:
REM    test.bat
REM
REM  Strategy:
REM    1. Build via build.bat (fails fast if compilation breaks).
REM    2. Run smoke test: sim.exe against data/sample_jobs.csv.
REM    3. Run edge cases: missing CSV arg, non-existent file.
REM    4. Print PASS/FAIL summary and exit non-zero if any test failed.
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
REM  Step 1: Build
REM ----------------------------------------------------------------------------
echo --- Step 1: Build ---
call build.bat
set BUILD_RC=%ERRORLEVEL%
if %BUILD_RC% NEQ 0 (
    echo.
    echo [FATAL] Build failed. Aborting tests.
    exit /b 1
)
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
