#pragma once
// ============================================================================
//  srtf.hpp  –  Shortest Remaining Time First (preemptive) scheduling policy
//
//  Preemptive: On every tick, picks the ready job with the shortest remaining
//  burst using a min-heap.
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

class Srtf final : public Scheduler {
public:
    const char* name() const noexcept override { return "SRTF"; }

    /// Preemptive: CPU can be preempted whenever a shorter job arrives.
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;

private:
    struct Compare {
        bool operator()(const Job* a, const Job* b) const noexcept {
            if (a->remaining != b->remaining) {
                return a->remaining > b->remaining; // min-heap: smaller remaining comes first
            }
            return a->id > b->id;                   // tie-break: smaller id comes first
        }
    };
};

} // namespace mlsched
