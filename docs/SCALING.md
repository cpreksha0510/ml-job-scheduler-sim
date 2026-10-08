# Scaling and Complexity Analysis

This report documents the empirical scaling behavior and algorithmic complexity of the ML Job Scheduler Simulator across workloads ranging from 1,000 to 100,000 jobs.

---

## 1. Environment & Hardware Specifications

- **CPU:** 12th Gen Intel(R) Core(TM) i5-1235U (10 cores: 2 Performance + 8 Efficient, 12 logical processors, base 1.30 GHz, boost up to 4.40 GHz)
- **RAM:** 16 GB DDR4
- **Operating System:** Windows 11 Home (64-bit)
- **Compiler:** `g++.exe 16.2.0` (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2)
- **Compilation Flags:** `-std=c++14 -O2 -Wall -Wextra -Iinclude -Isrc`
- **Timer:** `std::chrono::steady_clock` wrapped strictly around `sim.run(100'000'000)` (3 repetitions, median recorded)
- **Timeout Watchdog:** 120.0 seconds per run (none triggered)

---

## 2. Benchmark Results

### Simulation Runtimes and Throughput

Workload configurations generated with fixed `seed = 1`:
- **Medium 1k:** Horizon 6,711 $\to$ 1,014 actual jobs, 5,853 burst ticks
- **Medium 10k:** Horizon 67,114 $\to$ 9,950 actual jobs, 57,573 burst ticks
- **Medium 100k:** Horizon 671,140 $\to$ 99,949 actual jobs, 563,711 burst ticks
- **Overloaded 10k:** Horizon 39,682 $\to$ 10,042 actual jobs, 70,193 burst ticks

| Policy | Medium 1k (s) | Medium 10k (s) | Medium 100k (s) | Overloaded 10k (s) | Overloaded vs Med 10k Slowdown |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **FCFS** | 0.00076 | 0.00465 | 0.04004 | 0.25804 | **55.5x** |
| **SJF** | 0.00054 | 0.00512 | 0.04382 | 0.02069 | **4.0x** |
| **SRTF** | 0.00092 | 0.00833 | 0.07799 | 0.09098 | **10.9x** |
| **RR ($q=3$)** | 0.00035 | 0.00328 | 0.03472 | 0.00680 | **2.1x** |
| **MLFQ** | 0.00038 | 0.00354 | 0.03521 | 0.00550 | **1.6x** |
| **EDF** | 0.00087 | 0.00829 | 0.08197 | 0.25029 | **30.2x** |
| **Hybrid** | 0.00095 | 0.00961 | 0.08701 | 0.01916 | **2.0x** |

---

## 3. Scaling Ratio Analysis: Time(100k) / Time(10k)

For constant workload load (~80% capacity demand), scaling the job count by $10.04\times$ ($9,950 \to 99,949$) yields the following wall-clock scaling ratios:

| Policy | $T(\text{10k})$ (s) | $T(\text{100k})$ (s) | Ratio $\frac{T(\text{100k})}{T(\text{10k})}$ | Expected Ratio | Status |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **FCFS** | 0.00465 | 0.04004 | **8.60** | 10 -- 12 | Normal |
| **SJF** | 0.00512 | 0.04382 | **8.55** | 10 -- 12 | Normal |
| **SRTF** | 0.00833 | 0.07799 | **9.37** | 10 -- 12 | Normal |
| **RR** | 0.00328 | 0.03472 | **10.57** | 10 -- 12 | Normal |
| **MLFQ** | 0.00354 | 0.03521 | **9.95** | 10 -- 12 | Normal |
| **EDF** | 0.00829 | 0.08197 | **9.89** | 10 -- 12 | Normal |
| **Hybrid** | 0.00961 | 0.08701 | **9.06** | 10 -- 12 | Normal |

Under constant load, queue lengths remain bounded (peak ready queue was 172 across 100k jobs), so cache locality and CPU branch prediction keep all scaling ratios between 8.5x and 10.6x (near-perfect $O(n)$ behavior).

---

## 4. Algorithmic Complexity Evaluation & Identified Bottlenecks

While constant-load scaling appeared linear due to bounded queue depth, the **overloaded workload** reveals substantial performance divergence when ready queues grow long ($M \approx 4,188$ jobs).

### Summary of Theoretical vs. Empirical Complexity

| Policy | Spec Target | Actual Per-Tick Pick Complexity | Met Spec Target? |
| :--- | :---: | :---: | :---: |
| **FCFS** | $O(1)$ | $O(M)$ | **No** (linear scan every tick) |
| **SJF** | $O(\log n)$ | $O(M \log M)$ on dispatch, $O(1)$ during burst | **Partially** |
| **SRTF** | $O(\log n)$ | $O(M \log M)$ every tick | **No** (rebuilds priority queue every tick) |
| **RR** | $O(1)$ | $O(1)$ amortized | **Yes** |
| **MLFQ** | $O(1)$ | $O(1)$ amortized | **Yes** |
| **EDF** | $O(\log n)$ | $O(M \log M)$ every tick | **No** (rebuilds priority queue every tick) |
| **Hybrid** | $O(1)$ top tiers, $O(k)$ aging | $O(\|tier_0\|)$ scan + $O(\|tier_2\|)$ aging scan | **Partially** |

---

### Root Causes & Code Evidence

#### 1. FCFS: Linear Scan of Ready Queue Every Tick
- **File:** [`src/policies/fcfs.cpp`](file:///c:/sem%205/OSL/mini%20project/ml-job-scheduler-sim/src/policies/fcfs.cpp#L17-L21)
- **Function:** `Job* Fcfs::pick_next(std::vector<Job*>& ready, int64_t)`
- **Mechanism:**
  ```cpp
  return *std::min_element(ready.begin(), ready.end(),
      [](const Job* a, const Job* b) {
          if (a->arrival != b->arrival) return a->arrival < b->arrival;
          return a->id < b->id;
      });
  ```
- **Why it is slow:** In overloaded conditions ($M = 4,188$), `std::min_element` iterates over all 4,188 elements on every single tick. At 70,205 ticks, that requires ~290 million pointer dereferences and comparisons. Runtime exploded from 0.0046s to 0.258s (**55.5x slower**).
- **Target fix (if approved):** Track an internal FIFO queue or observe that jobs in `ready` are admitted in arrival order.

#### 2. EDF & SRTF: Priority Queue Reconstructed Every Tick
- **Files:** [`src/policies/edf.cpp`](file:///c:/sem%205/OSL/mini%20project/ml-job-scheduler-sim/src/policies/edf.cpp#L15-L20) and [`src/policies/srtf.cpp`](file:///c:/sem%205/OSL/mini%20project/ml-job-scheduler-sim/src/policies/srtf.cpp#L13-L18)
- **Functions:** `Job* Edf::pick_next(...)` and `Job* Srtf::pick_next(...)`
- **Mechanism:**
  ```cpp
  std::priority_queue<Job*, std::vector<Job*>, Compare> pq;
  for (Job* j : ready) {
      if (j != nullptr) pq.push(j);
  }
  return pq.top();
  ```
- **Why it is slow:** Rather than maintaining a persistent min-heap updated incrementally via `on_job_arrival` and `on_job_complete`, both policies allocate a heap vector and push all $M$ elements on *every tick*. For EDF in the overloaded run, this takes 0.250s (**30.2x slower**).
- **Target fix (if approved):** Maintain a persistent `std::priority_queue` or `std::set` updated on arrival/completion.

#### 3. Simulator Core: Linear Vector Erase on Completion
- **File:** [`src/simulator.cpp`](file:///c:/sem%205/OSL/mini%20project/ml-job-scheduler-sim/src/simulator.cpp#L99-L100)
- **Function:** `int64_t Simulator::run(...)`
- **Mechanism:**
  ```cpp
  ready_.erase(std::remove(ready_.begin(), ready_.end(), chosen), ready_.end());
  ```
- **Why it is slow:** `ready_` is a `std::vector<Job*>`. Removing a completed job requires scanning the vector ($O(M)$) and shifting elements left.
- **Target fix (if approved):** Swap with back (`std::swap(it, ready_.back()); ready_.pop_back();`) since order in `ready_` is not guaranteed by the contract.

---

## 5. Verification Checks

1. **Determinism at Scale:**
   - Evaluated Hybrid on the 100,000-job workload across two independent invocations.
   - Run 1 64-bit FNV-1a checksum: `0x5d49e2d98d224ad`
   - Run 2 64-bit FNV-1a checksum: `0x5d49e2d98d224ad`
   - **Result:** Byte-for-byte identical output verified.

2. **Sanity at Scale:**
   - Completed jobs equaled total submitted jobs for all 28 runs (100% completion rate).
   - Total busy ticks strictly matched total job bursts ($\sum \text{burst}_i$) across every policy and workload size. Zero dropped ticks or unaccounted executions.
