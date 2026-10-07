#pragma once
// ============================================================================
//  fcfs.hpp  –  First-Come First-Served scheduling policy
//
//  Non-preemptive. Picks the job with the smallest arrival time among those
//  currently in the ready queue.  O(n) scan (acceptable per SPEC complexity
//  targets; a FIFO queue would be O(1) but the Simulator manages the ready
//  vector, so we scan here to stay interface-clean).
//
//  Rules (AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler only.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

namespace mlsched {

class Fcfs final : public Scheduler {
public:
    // ── Scheduler interface ───────────────────────────────────────────────────

    const char* name() const noexcept override { return "FCFS"; }

    /// Non-preemptive: once a job is running, it keeps the CPU until done.
    bool is_preemptive() const noexcept override { return false; }

    /// Pick the job that arrived earliest.
    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;

    /// No per-tick internal state needed for plain FCFS.
    void on_tick(int64_t time) override;
};

} // namespace mlsched
