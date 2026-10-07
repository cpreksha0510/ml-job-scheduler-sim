#pragma once
// ============================================================================
//  scheduler.hpp  –  Abstract Scheduler interface
//  C++17, STL only.  Do NOT modify without reading AGENT_RULES.md first.
//
//  Every scheduling policy must:
//    1. Inherit from mlsched::Scheduler publicly.
//    2. Live in its own file under src/policies/.
//    3. Never touch the Simulator internals.
// ============================================================================
#include "job.hpp"

#include <vector>

namespace mlsched {

/// Abstract base class that every scheduling policy must implement.
class Scheduler {
public:
    virtual ~Scheduler() = default;

    // ── Core interface (must override) ────────────────────────────────────────

    /// Choose the next job to run.
    ///
    /// @param ready  Reference to the current ready queue (may be reordered
    ///               by the policy if desired, but jobs must not be erased).
    /// @param time   Current simulation tick.
    /// @return       Pointer to the selected job, or nullptr if none should run.
    ///
    /// Contract: the returned pointer must point to an element of @p ready
    /// (or be nullptr).  The Simulator owns all Job objects.
    virtual Job* pick_next(std::vector<Job*>& ready, int64_t time) = 0;

    /// Called once per tick, AFTER the chosen job has been executed for that
    /// tick.  Policies use this hook to update internal state (e.g., quantum
    /// counters, aging timers, MLFQ bookkeeping).
    ///
    /// @param time   Current simulation tick (same as passed to pick_next).
    virtual void on_tick(int64_t time) = 0;

    // ── Optional hooks (may override) ─────────────────────────────────────────

    /// Called whenever a new job is admitted to the system (arrives and passes
    /// resource checks).  Useful for policies that maintain sorted structures.
    virtual void on_job_arrival(Job& /*job*/, int64_t /*time*/) {}

    /// Called when a job completes.  Useful for policies that need to clean up
    /// per-job state.
    virtual void on_job_complete(Job& /*job*/, int64_t /*time*/) {}

    // ── Scheduler properties (may override) ──────────────────────────────────

    /// If true, the Simulator will preempt a running job whenever pick_next()
    /// returns a different job.  If false, the current job runs to completion
    /// (or until it blocks / exhausts its remaining time).
    virtual bool is_preemptive() const noexcept { return false; }

    /// Human-readable policy name for output headers.
    virtual const char* name() const noexcept { return "Unnamed"; }
};

} // namespace mlsched
