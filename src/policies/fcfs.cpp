// ============================================================================
//  fcfs.cpp  –  First-Come First-Served scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "fcfs.hpp"

#include <algorithm>

namespace mlsched {

void Fcfs::on_job_arrival(Job& job, int64_t /*time*/) {
    new_arrivals_.push_back(&job);
}

void Fcfs::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
    }
}

Job* Fcfs::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    // 1. Process new arrivals, sorting simultaneous arrivals by lowest job ID
    //    for determinism, matching textbook FCFS tie-breaking behaviour.
    if (!new_arrivals_.empty()) {
        std::sort(new_arrivals_.begin(), new_arrivals_.end(),
                  [](const Job* a, const Job* b) {
                      if (a->arrival != b->arrival) return a->arrival < b->arrival;
                      return a->id < b->id;
                  });
        for (Job* j : new_arrivals_) {
            queue_.push_back(j);
        }
        new_arrivals_.clear();
    }

    // 2. Non-preemptive: if current job is running and not complete, continue it.
    if (current_ != nullptr && !current_->is_complete()) {
        return current_;
    }
    current_ = nullptr;

    // 3. Pop next eligible job from the FIFO queue.
    while (!queue_.empty()) {
        Job* candidate = queue_.front();
        queue_.pop_front();
        if (candidate != nullptr && candidate->finish_time == UNSET && !candidate->is_complete()) {
            current_ = candidate;
            return current_;
        }
    }

    // 4. Fallback: if queue was empty but ready vector has jobs (e.g. direct test calls).
    if (current_ == nullptr && !ready.empty()) {
        auto it = std::min_element(ready.begin(), ready.end(),
            [](const Job* a, const Job* b) {
                if (a->arrival != b->arrival) return a->arrival < b->arrival;
                return a->id < b->id;
            });
        current_ = *it;
        return current_;
    }

    return nullptr;
}

void Fcfs::on_tick(int64_t /*time*/) {
    // Non-preemptive FCFS requires no per-tick bookkeeping.
}

} // namespace mlsched
