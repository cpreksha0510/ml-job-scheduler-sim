// ============================================================================
//  mlfq.cpp  –  Multi-Level Feedback Queue scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "mlfq.hpp"

#include <algorithm>

namespace mlsched {

Mlfq::Mlfq(int64_t q0, int64_t q1, int64_t q2, int64_t boost_period)
    : boost_period_(boost_period > 0 ? boost_period : 0)
{
    quanta_[0] = (q0 > 0 ? q0 : 2);
    quanta_[1] = (q1 > 0 ? q1 : 4);
    quanta_[2] = (q2 > 0 ? q2 : 8);
}

void Mlfq::on_job_arrival(Job& job, int64_t /*time*/) {
    new_arrivals_.push_back(&job);
}

void Mlfq::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
        ticks_used_ = 0;
    }
}

void Mlfq::on_tick(int64_t /*time*/) {
    if (current_ != nullptr) {
        ++ticks_used_;
    }
}

Job* Mlfq::pick_next(std::vector<Job*>& ready, int64_t time) {
    // 1. Process newly arrived jobs:
    //    "New arrivals enter queue 0 at the tail."
    //    "Tie at the same tick: arrivals are enqueued before a demoted or preempted job is requeued."
    //    Sort simultaneous arrivals by lowest job ID for determinism.
    if (!new_arrivals_.empty()) {
        std::sort(new_arrivals_.begin(), new_arrivals_.end(),
                  [](const Job* a, const Job* b) { return a->id < b->id; });
        for (Job* j : new_arrivals_) {
            queues_[0].push_back(j);
        }
        new_arrivals_.clear();
    }

    // 2. Handle quantum expiration for the currently running job:
    //    "If a job uses its full quantum and is not finished, demote it one level
    //     (stay in the last queue if already there) and put it at the tail.
    //     If it finishes exactly when the quantum ends, it just completes."
    if (current_ != nullptr && current_->finish_time != UNSET) {
        current_ = nullptr;
        ticks_used_ = 0;
    } else if (current_ != nullptr && ticks_used_ >= quanta_[current_level_]) {
        int next_level = (current_level_ < 2) ? (current_level_ + 1) : 2;
        queues_[next_level].push_back(current_);
        current_ = nullptr;
        ticks_used_ = 0;
    }

    // 3. Periodic priority boost:
    //    "Every S ticks (if S > 0), move all jobs to queue 0, keeping their relative order."
    if (boost_period_ > 0 && time > 0 && (time % boost_period_ == 0)) {
        if (current_ != nullptr) {
            // Requeue running job at its current queue level so all jobs move together
            queues_[current_level_].push_back(current_);
            current_ = nullptr;
            ticks_used_ = 0;
        }
        // Move all jobs from Q1 and Q2 to Q0, preserving relative order (Q0 then Q1 then Q2)
        for (Job* j : queues_[1]) {
            queues_[0].push_back(j);
        }
        queues_[1].clear();
        for (Job* j : queues_[2]) {
            queues_[0].push_back(j);
        }
        queues_[2].clear();
    }

    // 4. Handle preemption:
    //    "If a job arrives in a higher queue than the running job's queue,
    //     the running job is preempted. It stays at its SAME level (no demotion),
    //     goes to the tail, and its quantum resets on next dispatch."
    if (current_ != nullptr) {
        bool higher_job_present = false;
        for (int q = 0; q < current_level_; ++q) {
            if (!queues_[q].empty()) {
                higher_job_present = true;
                break;
            }
        }
        if (higher_job_present) {
            queues_[current_level_].push_back(current_);
            current_ = nullptr;
            ticks_used_ = 0;
        }
    }

    // 5. If no job is running, pick the head of the highest non-empty queue:
    //    "Always run the head of the highest non-empty queue."
    if (current_ == nullptr) {
        for (int q = 0; q < 3; ++q) {
            while (!queues_[q].empty()) {
                Job* candidate = queues_[q].front();
                queues_[q].pop_front();
                if (candidate != nullptr && candidate->finish_time == UNSET) {
                    current_ = candidate;
                    current_level_ = q;
                    ticks_used_ = 0;
                    break;
                }
            }
            if (current_ != nullptr) break;
        }
    }

    // 6. Fallback: if queues were empty but ready vector has jobs.
    if (current_ == nullptr && !ready.empty()) {
        for (Job* j : ready) {
            if (j != nullptr && j->finish_time == UNSET) {
                queues_[0].push_back(j);
            }
        }
        if (!queues_[0].empty()) {
            current_ = queues_[0].front();
            queues_[0].pop_front();
            current_level_ = 0;
            ticks_used_ = 0;
        }
    }

    return current_;
}

} // namespace mlsched
