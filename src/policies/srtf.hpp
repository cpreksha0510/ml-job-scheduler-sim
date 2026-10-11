#pragma once
// ============================================================================
//  srtf.hpp  –  Shortest Remaining Time First (preemptive) scheduling policy
//
//  Preemptive: Picks the ready job with the shortest remaining burst.
//  Tie-breaking: lowest job id wins.
//  Uses a persistent set updated on arrival/completion. The running job is
//  popped while executing so keys in the set are never mutated in place.
//
//  Rules (AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler only.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <set>
#include <vector>

namespace mlsched {

class Srtf final : public Scheduler {
public:
    const char* name() const noexcept override { return "SRTF"; }

    /// Preemptive: CPU can be preempted whenever a shorter job arrives.
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_arrival(Job& job, int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

private:
    struct Compare {
        bool operator()(const Job* a, const Job* b) const noexcept {
            if (a == b) return false;
            if (a->remaining != b->remaining) {
                return a->remaining < b->remaining; // smaller remaining comes first
            }
            return a->id < b->id;                   // tie-break: smaller id comes first
        }
    };

    Job* current_ = nullptr;
    std::set<Job*, Compare> waiting_set_;
};

} // namespace mlsched
