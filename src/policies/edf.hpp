#pragma once
// ============================================================================
//  edf.hpp  –  Earliest Deadline First (preemptive) scheduling policy
//
//  Preemptive. Min-heap on absolute deadline, ties broken by lowest job id.
//  Jobs with no deadline (deadline == UNSET / -1) are treated as infinitely
//  far away (lowest priority).
//  Jobs that have already missed their deadline are NOT dropped; they keep
//  running in deadline order.
//
//  Rules (SPEC.md / AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler publicly.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <queue>
#include <vector>

namespace mlsched {

class Edf final : public Scheduler {
public:
    const char* name() const noexcept override { return "EDF"; }
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;

private:
    struct Compare {
        bool operator()(const Job* a, const Job* b) const noexcept {
            bool a_has = a->has_deadline();
            bool b_has = b->has_deadline();

            if (!a_has && !b_has) {
                // Neither has deadline: tie-break by lowest job id
                return a->id > b->id;
            }
            if (!a_has && b_has) {
                // a has no deadline (infinitely far), so b comes first
                return true;
            }
            if (a_has && !b_has) {
                // a has deadline, so a comes first
                return false;
            }
            // Both have deadlines
            if (a->deadline != b->deadline) {
                return a->deadline > b->deadline; // min-heap: smaller deadline first
            }
            return a->id > b->id; // tie-break: smaller id first
        }
    };
};

} // namespace mlsched
