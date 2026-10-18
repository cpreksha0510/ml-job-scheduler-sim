#!/usr/bin/env python3
"""
test_cli.py - Regression test for sim.exe CLI mode.
Verifies that running sim.exe via command-line arguments on the 5-job benchmark
produces the exact same metrics as the existing unit tests (test_fcfs, test_sjf,
test_srtf, test_rr, test_mlfq), that per_job.csv, results.csv, and timeline.csv
are written correctly, and that bad arguments exit non-zero with clear errors.
"""

import os
import sys
import shutil
import tempfile
import subprocess
import pandas as pd

SIM_EXE = os.path.abspath("sim.exe")
BENCH_CSV = os.path.abspath("data/benchmark_5job.csv")

def run_sim(args, expect_success=True):
    cmd = [SIM_EXE] + args
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if expect_success and res.returncode != 0:
        print(f"FAILED: Command {' '.join(cmd)} failed with exit code {res.returncode}")
        print("STDERR:", res.stderr)
        sys.exit(1)
    if not expect_success and res.returncode == 0:
        print(f"FAILED: Expected command {' '.join(cmd)} to fail, but succeeded!")
        sys.exit(1)
    return res

def test_fcfs():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_fcfs_")
    try:
        run_sim(["--policy", "fcfs", "--workload", BENCH_CSV, "--out", temp_dir])
        pj = pd.read_csv(os.path.join(temp_dir, "per_job.csv")).sort_values("job_id")
        res = pd.read_csv(os.path.join(temp_dir, "results.csv"))
        assert os.path.exists(os.path.join(temp_dir, "timeline.csv")), "timeline.csv missing"

        # Check per-job waiting and turnaround
        exp_wait = [0, 4, 6, 13, 18]
        exp_ta   = [5, 7, 14, 19, 20]
        assert list(pj["waiting_time"]) == exp_wait, f"FCFS wait mismatch: {list(pj['waiting_time'])}"
        assert list(pj["turnaround_time"]) == exp_ta, f"FCFS ta mismatch: {list(pj['turnaround_time'])}"
        assert round(res["avg_wait"].iloc[0], 1) == 8.2, f"FCFS avg_wait: {res['avg_wait'].iloc[0]}"
        assert round(res["avg_turnaround"].iloc[0], 1) == 13.0, f"FCFS avg_ta: {res['avg_turnaround'].iloc[0]}"
        print("[PASS] CLI FCFS 5-job benchmark matches unit tests")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_sjf():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_sjf_")
    try:
        run_sim(["--policy", "sjf", "--workload", BENCH_CSV, "--out", temp_dir])
        pj = pd.read_csv(os.path.join(temp_dir, "per_job.csv")).sort_values("job_id")
        res = pd.read_csv(os.path.join(temp_dir, "results.csv"))

        exp_wait = [0, 6, 14, 7, 1]
        exp_ta   = [5, 9, 22, 13, 3]
        assert list(pj["waiting_time"]) == exp_wait, f"SJF wait mismatch: {list(pj['waiting_time'])}"
        assert list(pj["turnaround_time"]) == exp_ta, f"SJF ta mismatch: {list(pj['turnaround_time'])}"
        assert round(res["avg_wait"].iloc[0], 1) == 5.6, f"SJF avg_wait: {res['avg_wait'].iloc[0]}"
        assert round(res["avg_turnaround"].iloc[0], 1) == 10.4, f"SJF avg_ta: {res['avg_turnaround'].iloc[0]}"
        print("[PASS] CLI SJF 5-job benchmark matches unit tests")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_srtf():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_srtf_")
    try:
        run_sim(["--policy", "srtf", "--workload", BENCH_CSV, "--out", temp_dir])
        pj = pd.read_csv(os.path.join(temp_dir, "per_job.csv")).sort_values("job_id")
        res = pd.read_csv(os.path.join(temp_dir, "results.csv"))

        exp_wait = [5, 0, 14, 7, 0]
        exp_ta   = [10, 3, 22, 13, 2]
        exp_resp = [0, 0, 14, 7, 0]
        assert list(pj["waiting_time"]) == exp_wait, f"SRTF wait mismatch: {list(pj['waiting_time'])}"
        assert list(pj["turnaround_time"]) == exp_ta, f"SRTF ta mismatch: {list(pj['turnaround_time'])}"
        assert list(pj["response_time"]) == exp_resp, f"SRTF resp mismatch: {list(pj['response_time'])}"
        assert round(res["avg_wait"].iloc[0], 1) == 5.2, f"SRTF avg_wait: {res['avg_wait'].iloc[0]}"
        assert round(res["avg_turnaround"].iloc[0], 1) == 10.0, f"SRTF avg_ta: {res['avg_turnaround'].iloc[0]}"
        assert round(res["avg_response"].iloc[0], 1) == 4.2, f"SRTF avg_resp: {res['avg_response'].iloc[0]}"
        print("[PASS] CLI SRTF 5-job benchmark matches unit tests")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_rr():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_rr_")
    try:
        run_sim(["--policy", "rr", "--workload", BENCH_CSV, "--out", temp_dir, "--quantum", "2"])
        pj = pd.read_csv(os.path.join(temp_dir, "per_job.csv")).sort_values("job_id")
        res = pd.read_csv(os.path.join(temp_dir, "results.csv"))

        exp_wait = [11, 9, 14, 13, 6]
        exp_ta   = [16, 12, 22, 19, 8]
        exp_resp = [0, 1, 2, 5, 6]
        assert list(pj["waiting_time"]) == exp_wait, f"RR wait mismatch: {list(pj['waiting_time'])}"
        assert list(pj["turnaround_time"]) == exp_ta, f"RR ta mismatch: {list(pj['turnaround_time'])}"
        assert list(pj["response_time"]) == exp_resp, f"RR resp mismatch: {list(pj['response_time'])}"
        assert round(res["avg_wait"].iloc[0], 1) == 10.6, f"RR avg_wait: {res['avg_wait'].iloc[0]}"
        assert round(res["avg_turnaround"].iloc[0], 1) == 15.4, f"RR avg_ta: {res['avg_turnaround'].iloc[0]}"
        assert round(res["avg_response"].iloc[0], 1) == 2.8, f"RR avg_resp: {res['avg_response'].iloc[0]}"
        print("[PASS] CLI RR (q=2) 5-job benchmark matches unit tests")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_mlfq():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_mlfq_")
    try:
        run_sim(["--policy", "mlfq", "--workload", BENCH_CSV, "--out", temp_dir])
        pj = pd.read_csv(os.path.join(temp_dir, "per_job.csv")).sort_values("job_id")

        exp_finish = [13, 14, 24, 22, 10]
        exp_wait   = [8, 10, 14, 13, 4]
        exp_ta     = [13, 13, 22, 19, 6]
        exp_resp   = [0, 1, 2, 3, 4]
        assert list(pj["finish_time"]) == exp_finish, f"MLFQ finish mismatch: {list(pj['finish_time'])}"
        assert list(pj["waiting_time"]) == exp_wait, f"MLFQ wait mismatch: {list(pj['waiting_time'])}"
        assert list(pj["turnaround_time"]) == exp_ta, f"MLFQ ta mismatch: {list(pj['turnaround_time'])}"
        assert list(pj["response_time"]) == exp_resp, f"MLFQ resp mismatch: {list(pj['response_time'])}"
        print("[PASS] CLI MLFQ 5-job benchmark matches unit tests")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_hybrid_and_knobs():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_hybrid_")
    try:
        run_sim([
            "--policy", "hybrid",
            "--workload", BENCH_CSV,
            "--out", temp_dir,
            "--tier1-quantum", "4",
            "--tier2-quantum", "16",
            "--aging", "50",
            "--reserve-k", "5"
        ])
        assert os.path.exists(os.path.join(temp_dir, "per_job.csv"))
        assert os.path.exists(os.path.join(temp_dir, "results.csv"))
        assert os.path.exists(os.path.join(temp_dir, "timeline.csv"))
        print("[PASS] CLI Hybrid with custom knobs runs and exports CSVs")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_error_handling():
    temp_dir = tempfile.mkdtemp(prefix="mlsched_cli_err_")
    try:
        # 1. Missing policy
        r = run_sim(["--workload", BENCH_CSV, "--out", temp_dir], expect_success=False)
        assert "Missing required argument: --policy" in r.stderr or "Error" in r.stderr

        # 2. Unknown policy
        r = run_sim(["--policy", "unknown_policy", "--workload", BENCH_CSV, "--out", temp_dir], expect_success=False)
        assert "Unknown policy" in r.stderr

        # 3. Non-existent workload
        r = run_sim(["--policy", "fcfs", "--workload", "data/non_existent.csv", "--out", temp_dir], expect_success=False)
        assert "Error loading workload CSV" in r.stderr or "Cannot open" in r.stderr

        # 4. Bad integer for quantum
        r = run_sim(["--policy", "rr", "--workload", BENCH_CSV, "--out", temp_dir, "--quantum", "-5"], expect_success=False)
        assert "positive integer" in r.stderr

        print("[PASS] CLI error handling tests pass with clear stderr messages")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

if __name__ == "__main__":
    print("\n=== Running CLI Mode Regression Tests ===")
    test_fcfs()
    test_sjf()
    test_srtf()
    test_rr()
    test_mlfq()
    test_hybrid_and_knobs()
    test_error_handling()
    print("\n[ALL CLI TESTS PASSED]\n")
