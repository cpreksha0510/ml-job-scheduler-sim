// ============================================================================
//  rr.cpp  –  Round Robin scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "rr.hpp"

#include <algorithm>

namespace mlsched {

RoundRobin::RoundRobin(int64_t quantum)
    : quantum_(quantum > 0 ? quantum : 1)
{
}

void RoundRobin::on_job_arrival(Job& job, int64_t /*time*/) {
    new_arrivals_.push_back(&job);
}

void RoundRobin::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
        ticks_used_ = 0;
    }
}

void RoundRobin::on_tick(int64_t /*time*/) {
    if (current_ != nullptr) {
        ++ticks_used_;
    }
}

Job* RoundRobin::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    // 1. Process any jobs that arrived this tick.
    //    Sort simultaneous arrivals by lowest job ID so they join the queue in order.
    if (!new_arrivals_.empty()) {
        std::sort(new_arrivals_.begin(), new_arrivals_.end(),
                  [](const Job* a, const Job* b) { return a->id < b->id; });
        for (Job* j : new_arrivals_) {
            queue_.push_back(j);
        }
        new_arrivals_.clear();
    }

    // 2. Handle quantum expiration for the current job.
    if (current_ != nullptr && current_->finish_time != UNSET) {
        current_ = nullptr;
        ticks_used_ = 0;
    } else if (current_ != nullptr && ticks_used_ >= quantum_) {
        // Quantum expired: re-enqueue preempted job to the back of the queue.
        // Since new arrivals were enqueued in step 1, the new arrival is enqueued
        // first, then the preempted job.
        queue_.push_back(current_);
        current_ = nullptr;
        ticks_used_ = 0;
    }

    // 3. If no job is currently scheduled, pop the next eligible job from queue_.
    if (current_ == nullptr) {
        while (!queue_.empty()) {
            Job* candidate = queue_.front();
            queue_.pop_front();
            if (candidate != nullptr && candidate->finish_time == UNSET) {
                current_ = candidate;
                ticks_used_ = 0;
                break;
            }
        }
    }

    // 4. Fallback: if queue was empty but ready vector has jobs.
    if (current_ == nullptr && !ready.empty()) {
        for (Job* j : ready) {
            if (j != nullptr && j->finish_time == UNSET) {
                queue_.push_back(j);
            }
        }
        if (!queue_.empty()) {
            current_ = queue_.front();
            queue_.pop_front();
            ticks_used_ = 0;
        }
    }

    return current_;
}

} // namespace mlsched
