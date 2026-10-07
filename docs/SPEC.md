# ML-Workload-Aware Job Scheduler Simulator

**Title:** Deadline- and Resource-Aware CPU/GPU Scheduling for Mixed ML
Training and Inference Workloads

## Problem
Classic schedulers (FCFS, SJF, Round Robin) assume independent CPU-bound
jobs. Real ML infrastructure runs a mix: long training jobs
(throughput-bound), short latency-sensitive inference requests with
deadlines, and batch preprocessing. A naive scheduler lets training
starve inference or packs resources poorly. This project builds a
simulator that models such workloads and compares scheduling policies.

## Objectives
1. Workload generator with three job classes (training, inference,
   preprocessing) with distinct arrival patterns, burst lengths,
   priorities, and deadlines.
2. Implement FCFS, SJF/SRTF, Round Robin, MLFQ, EDF, and a custom
   hybrid policy.
3. Measure: avg waiting time, turnaround time, response time,
   deadline-miss rate, throughput, starvation.
4. Show where the hybrid beats the classic policies and where it doesn't.

## Design
- **Job struct:** id, class, arrival, burst, remaining, priority,
  deadline, mem_req.
- **Workload generator:** Poisson arrivals for inference, sparse long
  bursts for training, seeded RNG for reproducibility.
- **Scheduler interface:** each policy implements
  pick_next(ready_queue, time) and on_tick(), so policies plug in
  without touching the simulator core.
- **Hybrid policy (original contribution):** MLFQ with three tiers.
  Inference jobs enter the top tier ordered by EDF. Training jobs sit
  in lower tiers with long quanta. Aging promotes starved jobs after a
  threshold. A small reserved capacity guarantees training progress.
- **Metrics module:** per-job timeline, exportable as Gantt charts.
- **Output:** CSV results plus Python (matplotlib) graphs.

## Complexity targets
- FCFS: O(1) pick. SJF/EDF with a min-heap: O(log n) per pick.
  MLFQ: O(1) amortized per pick, O(k) per aging scan for k queues.
- Space: O(n) for jobs, O(1) extra per queue.

## Test plan
- Hand-verified small cases (textbook 5-job example).
- Edge cases: simultaneous arrivals, zero-burst jobs, identical jobs,
  one huge job.
- Stress runs with 10k and 100k jobs.
- Compare FCFS and SJF averages against textbook formulas.

## Constraints
- C++17, STL only, CMake build.
- Same seed must always produce the same workload and results.