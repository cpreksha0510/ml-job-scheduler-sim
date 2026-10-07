#pragma once
// ============================================================================
//  sjf.hpp  –  Shortest Job First (non-preemptive) scheduling policy
//
//  Non-preemptive: Once a job starts running, it executes until completion.
//  Picks the ready job with the shortest initial burst using a min-heap.
//  Tie-breaking: lowest job id wins.
//
//  Rules (AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler only.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <queue>
#include <vector>

namespace mlsched {

class Sjf final : public Scheduler {
public:
    const char* name() const noexcept override { return "SJF"; }

    /// Non-preemptive: once scheduled, runs until completion.
    bool is_preemptive() const noexcept override { return false; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

private:
    struct Compare {
        bool operator()(const Job* a, const Job* b) const noexcept {
            if (a->burst != b->burst) {
                return a->burst > b->burst; // min-heap: smaller burst comes first
            }
            return a->id > b->id;           // tie-break: smaller id comes first
        }
    };

    Job* current_ = nullptr;
};

} // namespace mlsched
