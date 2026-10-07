# src/policies/

Scheduling policy implementations live here.
Each policy:
- Is in its own `.cpp` / `.hpp` pair named after the policy (e.g. `fcfs.hpp`).
- Inherits publicly from `mlsched::Scheduler` (defined in `include/scheduler.hpp`).
- **Never** touches `Simulator` internals.
- New policy `.cpp` files are picked up automatically by `build.bat` (`src/policies/*.cpp`).

## Planned policies
| File            | Policy                         |
|-----------------|--------------------------------|
| `fcfs.hpp`      | First-Come First-Served        |
| `sjf.hpp`       | Shortest Job First (non-preemptive) |
| `srtf.hpp`      | Shortest Remaining Time First  |
| `rr.hpp`        | Round Robin                    |
| `mlfq.hpp`      | Multi-Level Feedback Queue     |
| `edf.hpp`       | Earliest Deadline First        |
| `hybrid.hpp`    | Custom hybrid (MLFQ + EDF)     |
