#pragma once
// ============================================================================
//  rr.hpp  –  Round Robin scheduling policy
//
//  Preemptive. Jobs are scheduled in FIFO order with a fixed time quantum.
//  Tie-breaking: lowest job id wins.
//  If a job's quantum expires at the same tick a new job arrives, the new
//  arrival is enqueued first, then the preempted job.
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

class RoundRobin final : public Scheduler {
public:
    explicit RoundRobin(int64_t quantum = 1);

    const char* name() const noexcept override { return "Round Robin"; }
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_arrival(Job& job, int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

    int64_t quantum() const noexcept { return quantum_; }

private:
    int64_t quantum_;
    int64_t ticks_used_ = 0;
    Job* current_ = nullptr;
    std::deque<Job*> queue_;
    std::vector<Job*> new_arrivals_;
};

} // namespace mlsched
