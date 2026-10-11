#pragma once
// ============================================================================
//  edf.hpp  –  Earliest Deadline First (preemptive) scheduling policy
//
//  Preemptive. Persistent set on absolute deadline, ties broken by lowest job id.
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

#include <set>
#include <vector>

namespace mlsched {

class Edf final : public Scheduler {
public:
    const char* name() const noexcept override { return "EDF"; }
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_arrival(Job& job, int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

private:
    struct Compare {
        bool operator()(const Job* a, const Job* b) const noexcept {
            if (a == b) return false;
            bool a_has = a->has_deadline();
            bool b_has = b->has_deadline();

            if (!a_has && !b_has) {
                return a->id < b->id;
            }
            if (!a_has && b_has) {
                return false;
            }
            if (a_has && !b_has) {
                return true;
            }
            if (a->deadline != b->deadline) {
                return a->deadline < b->deadline; // earlier deadline comes first
            }
            return a->id < b->id;                 // tie-break: smaller id first
        }
    };

    std::set<Job*, Compare> ready_set_;
};

} // namespace mlsched
