@echo off
REM ============================================================================
REM  test.bat  --  Run integration tests for the ML Job Scheduler Simulator
REM
REM  Usage:
REM    test.bat
REM
REM  Strategy:
REM    1. Build sim.exe via build.bat (fails fast if compilation breaks).
REM    2. Build and run tests\test_fcfs      (FCFS unit tests, plain asserts).
REM    3. Build and run tests\test_sjf       (SJF unit tests, plain asserts).
REM    4. Build and run tests\test_srtf      (SRTF unit tests, plain asserts).
REM    5. Build and run tests\test_rr        (Round Robin unit tests, plain asserts).
REM    6. Build and run tests\test_metrics   (Metrics module unit tests).
REM    7. Build and run tests\test_workload  (WorkloadGenerator unit tests).
REM    8. Build and run tests\test_mlfq      (MLFQ unit tests, plain asserts).
REM    9. Build and run tests\test_edf       (EDF unit tests, plain asserts).
REM   10. Build and run tests\test_hybrid    (Hybrid unit tests, plain asserts).
REM   11. Run smoke tests: sim.exe on CSV, CLI options, generator flag.
REM   12. Run edge cases: missing CSV arg, non-existent file.
REM   13. Print PASS/FAIL summary and exit non-zero if any test failed.
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
REM ----------------------------------------------------------------------------
echo --- Step 2: FCFS unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\fcfs.cpp tests\test_fcfs.cpp -o test_fcfs
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
REM  Step 3: Build and run SJF unit tests
REM ----------------------------------------------------------------------------
echo --- Step 3: SJF unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\sjf.cpp tests\test_sjf.cpp -o test_sjf
if %ERRORLEVEL% NEQ 0 goto :sjf_compile_fail

test_sjf.exe
if %ERRORLEVEL% EQU 0 goto :sjf_pass

echo [FAIL] test_sjf returned a non-zero exit code.
set /a FAIL+=1
goto :sjf_done

:sjf_compile_fail
echo [FAIL] test_sjf.cpp failed to compile.
set /a FAIL+=1
goto :sjf_done

:sjf_pass
set /a PASS+=1

:sjf_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 4: Build and run SRTF unit tests
REM ----------------------------------------------------------------------------
echo --- Step 4: SRTF unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\srtf.cpp tests\test_srtf.cpp -o test_srtf
if %ERRORLEVEL% NEQ 0 goto :srtf_compile_fail

test_srtf.exe
if %ERRORLEVEL% EQU 0 goto :srtf_pass

echo [FAIL] test_srtf returned a non-zero exit code.
set /a FAIL+=1
goto :srtf_done

:srtf_compile_fail
echo [FAIL] test_srtf.cpp failed to compile.
set /a FAIL+=1
goto :srtf_done

:srtf_pass
set /a PASS+=1

:srtf_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 5: Build and run Round Robin unit tests
REM ----------------------------------------------------------------------------
echo --- Step 5: Round Robin unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\rr.cpp tests\test_rr.cpp -o test_rr
if %ERRORLEVEL% NEQ 0 goto :rr_compile_fail

test_rr.exe
if %ERRORLEVEL% EQU 0 goto :rr_pass

echo [FAIL] test_rr returned a non-zero exit code.
set /a FAIL+=1
goto :rr_done

:rr_compile_fail
echo [FAIL] test_rr.cpp failed to compile.
set /a FAIL+=1
goto :rr_done

:rr_pass
set /a PASS+=1

:rr_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 6: Build and run Metrics unit tests
REM ----------------------------------------------------------------------------
echo --- Step 6: Metrics unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\fcfs.cpp src\policies\rr.cpp tests\test_metrics.cpp -o test_metrics
if %ERRORLEVEL% NEQ 0 goto :metrics_compile_fail

test_metrics.exe
if %ERRORLEVEL% EQU 0 goto :metrics_pass

echo [FAIL] test_metrics returned a non-zero exit code.
set /a FAIL+=1
goto :metrics_done

:metrics_compile_fail
echo [FAIL] test_metrics.cpp failed to compile.
set /a FAIL+=1
goto :metrics_done

:metrics_pass
set /a PASS+=1

:metrics_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 7: Build and run WorkloadGenerator unit tests
REM ----------------------------------------------------------------------------
echo --- Step 7: WorkloadGenerator unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\workload.cpp tests\test_workload.cpp -o test_workload
if %ERRORLEVEL% NEQ 0 goto :workload_compile_fail

test_workload.exe
if %ERRORLEVEL% EQU 0 goto :workload_pass

echo [FAIL] test_workload returned a non-zero exit code.
set /a FAIL+=1
goto :workload_done

:workload_compile_fail
echo [FAIL] test_workload.cpp failed to compile.
set /a FAIL+=1
goto :workload_done

:workload_pass
set /a PASS+=1

:workload_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 8: Build and run MLFQ unit tests
REM ----------------------------------------------------------------------------
echo --- Step 8: MLFQ unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\mlfq.cpp tests\test_mlfq.cpp -o test_mlfq
if %ERRORLEVEL% NEQ 0 goto :mlfq_compile_fail

test_mlfq.exe
if %ERRORLEVEL% EQU 0 goto :mlfq_pass

echo [FAIL] test_mlfq returned a non-zero exit code.
set /a FAIL+=1
goto :mlfq_done

:mlfq_compile_fail
echo [FAIL] test_mlfq.cpp failed to compile.
set /a FAIL+=1
goto :mlfq_done

:mlfq_pass
set /a PASS+=1

:mlfq_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 9: Build and run EDF unit tests
REM ----------------------------------------------------------------------------
echo --- Step 9: EDF unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\policies\edf.cpp tests\test_edf.cpp -o test_edf
if %ERRORLEVEL% NEQ 0 goto :edf_compile_fail

test_edf.exe
if %ERRORLEVEL% EQU 0 goto :edf_pass

echo [FAIL] test_edf returned a non-zero exit code.
set /a FAIL+=1
goto :edf_done

:edf_compile_fail
echo [FAIL] test_edf.cpp failed to compile.
set /a FAIL+=1
goto :edf_done

:edf_pass
set /a PASS+=1

:edf_done
echo.

REM ----------------------------------------------------------------------------
REM  Step 10: Build and run Hybrid unit tests
REM ----------------------------------------------------------------------------
echo --- Step 10: Hybrid unit tests ---
g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc src\simulator.cpp src\metrics.cpp src\workload.cpp src\policies\hybrid.cpp tests\test_hybrid.cpp -o test_hybrid
if %ERRORLEVEL% NEQ 0 goto :hybrid_compile_fail

test_hybrid.exe
if %ERRORLEVEL% EQU 0 goto :hybrid_pass

echo [FAIL] test_hybrid returned a non-zero exit code.
set /a FAIL+=1
goto :hybrid_done

:hybrid_compile_fail
echo [FAIL] test_hybrid.cpp failed to compile.
set /a FAIL+=1
goto :hybrid_done

:hybrid_pass
set /a PASS+=1

:hybrid_done
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
REM  Test 3: Generator CLI flag (expect exit 0)
REM ----------------------------------------------------------------------------
echo --- Test 3: Generator CLI flag ---
sim.exe --generate data\workload_light.cfg data\generated_light.csv > nul 2>&1
call :check_result 0 %ERRORLEVEL% "Generator CLI: sim --generate runs on light config"
echo.

REM ----------------------------------------------------------------------------
REM  Test 4: No arguments -- should exit non-zero (usage error)
REM ----------------------------------------------------------------------------
echo --- Test 4: No arguments (expect non-zero) ---
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
REM  Test 5: Non-existent CSV -- should exit non-zero
REM ----------------------------------------------------------------------------
echo --- Test 5: Non-existent file (expect non-zero) ---
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
