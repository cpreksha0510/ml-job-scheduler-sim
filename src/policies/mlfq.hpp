#pragma once
// ============================================================================
//  mlfq.hpp  –  Multi-Level Feedback Queue scheduling policy
//
//  Preemptive. 3 priority queues (std::deque each) with configurable quanta
//  (default 2, 4, 8) and an optional priority boost period S (0 disables boosting).
//
//  Rules (SPEC.md / AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler publicly.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <cstddef>
#include <deque>
#include <vector>

namespace mlsched {

class Mlfq final : public Scheduler {
public:
    explicit Mlfq(int64_t q0 = 2, int64_t q1 = 4, int64_t q2 = 8, int64_t boost_period = 0);

    const char* name() const noexcept override { return "MLFQ"; }
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_arrival(Job& job, int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

    int64_t quantum(size_t level) const noexcept {
        return (level < 3) ? quanta_[level] : 0;
    }
    int64_t boost_period() const noexcept { return boost_period_; }

private:
    int64_t quanta_[3];
    int64_t boost_period_;
    int64_t ticks_used_ = 0;
    int current_level_ = 0;
    Job* current_ = nullptr;

    std::deque<Job*> queues_[3];
    std::vector<Job*> new_arrivals_;
};

} // namespace mlsched
