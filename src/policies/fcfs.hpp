#pragma once
// ============================================================================
//  fcfs.hpp  –  First-Come First-Served scheduling policy
//
//  Non-preemptive. Picks the job with the smallest arrival time among those
//  currently in the ready queue. Uses an internal FIFO queue populated via
//  on_job_arrival for O(1) dispatch.
//
//  Rules (AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler only.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <deque>
#include <vector>

namespace mlsched {

class Fcfs final : public Scheduler {
public:
    const char* name() const noexcept override { return "FCFS"; }

    /// Non-preemptive: once a job is running, it keeps the CPU until done.
    bool is_preemptive() const noexcept override { return false; }

    /// Pick the job that arrived earliest.
    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;

    /// No per-tick internal state needed for plain FCFS.
    void on_tick(int64_t time) override;

    /// Enqueue newly admitted jobs in FIFO arrival order.
    void on_job_arrival(Job& job, int64_t time) override;

    /// Clean up current job pointer upon completion.
    void on_job_complete(Job& job, int64_t time) override;

private:
    Job* current_ = nullptr;
    std::deque<Job*> queue_;
    std::vector<Job*> new_arrivals_;
};

} // namespace mlsched
