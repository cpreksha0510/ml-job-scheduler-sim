# Scaling and Complexity Analysis

This report documents the empirical scaling behavior and algorithmic complexity of the ML Job Scheduler Simulator across workloads ranging from 1,000 to 100,000 jobs, including performance before and after algorithmic optimizations.

---

## 1. Environment & Hardware Specifications

- **CPU:** 12th Gen Intel(R) Core(TM) i5-1235U (10 cores: 2 Performance + 8 Efficient, 12 logical processors, base 1.30 GHz, boost up to 4.40 GHz)
- **RAM:** 16 GB DDR4
- **Operating System:** Windows 11 Home (64-bit)
- **Compiler:** `g++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2) 16.2.0`
- **Compilation Flags:** `-std=c++14 -O2 -Wall -Wextra -Iinclude -Isrc`
- **Timer:** `std::chrono::steady_clock` wrapped strictly around `sim.run(100'000'000)` (3 repetitions, median recorded)
- **Timeout Watchdog:** 120.0 seconds per run (none triggered)
- **Quantum Configuration Note:** The scaling benchmark uses Round Robin with quantum $q=3$ (`RR`), whereas the comparative benchmark experiments (`experiments.exe`) evaluate two quantum sizes: $q=2$ (`RR-small`, matching median inference burst) and $q=8$ (`RR-large`, matching Hybrid Tier 1 quantum).

---

## 2. Benchmark Results

### Workload Configurations (seed = 1)
- **Medium 1k:** Horizon 6,711 $\to$ 1,014 actual jobs, 5,853 burst ticks
- **Medium 10k:** Horizon 67,114 $\to$ 9,950 actual jobs, 57,573 burst ticks
- **Medium 100k:** Horizon 671,140 $\to$ 99,949 actual jobs, 563,711 burst ticks
- **Overloaded 10k:** Horizon 39,682 $\to$ 10,042 actual jobs, 70,193 burst ticks (peak ready queue up to 4,188 jobs)

---

### Before Optimisation

Prior to algorithmic improvements, FCFS performed linear queue scans each tick, EDF and SRTF rebuilt a priority queue from scratch on every tick, the Simulator core shifted vector elements on job completion, and Hybrid performed linear scans over Tier 0 and Tier 2.

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

### After Optimisation

Following the approved fixes (persistent sets, FIFO arrival queue for FCFS, swap-and-pop vector removal in the Simulator core, persistent set for Hybrid Tier 0, and $O(1)$ monotone queue head aging check for Hybrid Tier 2):

| Policy | Medium 1k (s) | Medium 10k (s) | Medium 100k (s) | Overloaded 10k (s) | Overloaded vs Med 10k Slowdown |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **FCFS** | 0.00035 | 0.00334 | 0.03302 | 0.00522 | **1.56x** |
| **SJF** | 0.00047 | 0.00442 | 0.04387 | 0.02135 | **4.83x** |
| **SRTF** | 0.00045 | 0.00418 | 0.03931 | 0.00506 | **1.21x** |
| **RR ($q=3$)** | 0.00037 | 0.00379 | 0.03564 | 0.00435 | **1.15x** |
| **MLFQ** | 0.00040 | 0.00374 | 0.03448 | 0.00550 | **1.47x** |
| **EDF** | 0.00041 | 0.00446 | 0.03874 | 0.00579 | **1.30x** |
| **Hybrid** | 0.00039 | 0.00385 | 0.04197 | 0.00554 | **1.44x** |

---

### Before vs. After Comparison (Overloaded 10k Workload)

Under severe queue buildup ($M = 4,188$ ready jobs), eliminating per-tick scans and rebuilds produces massive wall-clock speedups and eliminates artificial slowdown spikes:

| Policy | Before 10k Overloaded (s) | After 10k Overloaded (s) | Speedup Factor | Before Slowdown vs Med 10k | After Slowdown vs Med 10k |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **FCFS** | 0.25804 | 0.00522 | **49.4x** | 55.5x | **1.56x** |
| **EDF** | 0.25029 | 0.00579 | **43.2x** | 30.2x | **1.30x** |
| **SRTF** | 0.09098 | 0.00506 | **18.0x** | 10.9x | **1.21x** |
| **Hybrid** | 0.01916 | 0.00554 | **3.46x** | 2.0x | **1.44x** |
| **RR ($q=3$)** | 0.00680 | 0.00435 | **1.56x** | 2.1x | **1.15x** |

---

## 3. Scaling Ratio Analysis: Time(100k) / Time(10k)

For constant workload load (~80% capacity demand), scaling the job count by $10.04\times$ ($9,950 \to 99,949$) yields the following scaling ratios:

| Policy | $T(\text{10k})$ (s) | $T(\text{100k})$ (s) | Ratio $\frac{T(\text{100k})}{T(\text{10k})}$ | Expected Ratio | Status |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **FCFS** | 0.00334 | 0.03302 | **9.88** | 10 -- 12 | Normal |
| **SJF** | 0.00442 | 0.04387 | **9.93** | 10 -- 12 | Normal |
| **SRTF** | 0.00418 | 0.03931 | **9.41** | 10 -- 12 | Normal |
| **RR ($q=3$)** | 0.00379 | 0.03564 | **9.40** | 10 -- 12 | Normal |
| **MLFQ** | 0.00374 | 0.03448 | **9.21** | 10 -- 12 | Normal |
| **EDF** | 0.00446 | 0.03874 | **8.68** | 10 -- 12 | Normal |
| **Hybrid** | 0.00385 | 0.04197 | **10.90** | 10 -- 12 | Normal |

All policies scale linearly across two orders of magnitude with empirical ratios between 8.7x and 10.9x.

---

## 4. Algorithmic Complexity Evaluation

### Updated Theoretical vs. Empirical Complexity Table

| Policy | Spec Target | Before Optimisation Complexity | After Optimisation Complexity | Met Spec Target? |
| :--- | :---: | :---: | :---: | :---: |
| **FCFS** | $O(1)$ | $O(M)$ per tick (`std::min_element`) | $O(1)$ amortized dispatch via internal FIFO queue | **Yes** |
| **SJF** | $O(\log n)$ | $O(M \log M)$ on dispatch, $O(1)$ during burst | $O(M \log M)$ on dispatch, $O(1)$ during burst | **Partially** |
| **SRTF** | $O(\log n)$ | $O(M \log M)$ every tick (heap rebuild) | $O(1)$ during execution, $O(\log M)$ arrival/preemption via persistent set | **Yes** |
| **RR** | $O(1)$ | $O(1)$ amortized | $O(1)$ amortized | **Yes** |
| **MLFQ** | $O(1)$ | $O(1)$ amortized | $O(1)$ amortized | **Yes** |
| **EDF** | $O(\log n)$ | $O(M \log M)$ every tick (heap rebuild) | $O(1)$ pick, $O(\log M)$ arrival/completion via persistent set | **Yes** |
| **Hybrid** | $O(1)$ top tiers, $O(k)$ aging | $O(\|tier_0\|)$ scan + $O(\|tier_2\|)$ aging scan | $O(\log \|tier_0\|)$ set, $O(1)$ queue head aging check | **Yes** |

---

### Root Causes & Optimization Details

#### 1. FCFS: Eliminated Per-Tick Linear Scan
- **File:** [`src/policies/fcfs.cpp`](../src/policies/fcfs.cpp)
- **Previous Bottleneck:** Evaluated `std::min_element` over `ready` on every simulation tick ($O(M)$), costing 290M pointer dereferences in overloaded runs (runtime 0.258s).
- **Optimization:** Replaced with an internal `std::deque<Job*>` populated on admission via `on_job_arrival`. Dispatch popped from the head in $O(1)$ time. Non-preemptive execution returns the running pointer in $O(1)$. Overloaded runtime dropped from 0.258s to **0.0052s** (**49.4x speedup**).

#### 2. EDF & SRTF: Persistent Sets Replaced Per-Tick Heap Rebuilds
- **Files:** [`src/policies/edf.cpp`](../src/policies/edf.cpp) and [`src/policies/srtf.cpp`](../src/policies/srtf.cpp)
- **Previous Bottleneck:** Reconstructed a `std::priority_queue` containing all $M$ ready jobs on every single tick ($O(M \log M)$ per tick).
- **Optimization:**
  - **EDF:** Maintains a persistent `std::set<Job*, Compare>` ordered by `(has_deadline, deadline, id)`. Insertions occur on arrival ($O(\log M)$) and removals on completion ($O(\log M)$). `pick_next` returns `*ready_set_.begin()` in $O(1)$ time.
  - **SRTF:** Maintains a persistent `std::set<Job*, Compare>` of waiting jobs keyed by remaining burst and job id. The running job is popped while executing; when preempted or stopped, it is re-inserted with its updated remaining time. No key is ever mutated in place inside the set.
  - Overloaded runtime dropped from 0.250s to **0.0058s** for EDF (**43.2x speedup**) and from 0.091s to **0.0051s** for SRTF (**18.0x speedup**).

#### 3. Simulator Core: Swap-and-Pop Vector Removal
- **File:** [`src/simulator.cpp`](../src/simulator.cpp)
- **Previous Bottleneck:** Erased completed jobs via `ready_.erase(std::remove(...))` which shifted elements left on vector removal ($O(M)$ moves).
- **Optimization:** Replaced with swap-and-pop (`*it = ready_.back(); ready_.pop_back();`), avoiding element shifting. This optimization is strictly valid because no scheduling policy depends on the order of `ready_`.

#### 4. Hybrid: Tier 0 Persistent Set & Monotone Queue Head Aging Check
- **Files:** [`src/policies/hybrid.cpp`](../src/policies/hybrid.cpp) and [`src/policies/hybrid.hpp`](../src/policies/hybrid.hpp)
- **Previous Bottleneck:** Scanned Tier 0 linearly on every tick for preemption and dispatch ($O(|tier_0|)$); scanned all of Tier 2 and updated a hash map for aging promotions on every tick ($O(|tier_2|)$).
- **Optimization:**
  - **Tier 0:** Implemented as a persistent `std::set<Job*, Tier0Compare>`. Preemption checks compare the running job against `*tier0_.begin()` in $O(1)$.
  - **Tier 2:** Replaced the full queue scan with a queue head aging check. Because jobs only enter at the back at the current simulation tick, `enqueue_tick` is monotonically non-decreasing along the FIFO queue, making elapsed wait time monotonically non-increasing from head to tail. Checking only `tier2_.front()` in $O(1)$ is mathematically equivalent to scanning all elements.

---

## 5. Verification Checks

1. **Determinism at Scale:**
   - Evaluated Hybrid on the 100,000-job workload across two independent invocations.
   - Run 1 64-bit FNV-1a checksum: `0x5d49e2d98d224ad`
   - Run 2 64-bit FNV-1a checksum: `0x5d49e2d98d224ad`
   - **Result:** Byte-for-byte identical output verified.

2. **Sanity at Scale:**
   - Completed jobs equaled total submitted jobs for all 28 benchmark runs (100% completion rate).
   - Total busy ticks strictly matched total job bursts ($\sum \text{burst}_i$) across every policy and workload size. Zero dropped ticks or unaccounted executions.
