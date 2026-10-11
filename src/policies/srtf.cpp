// ============================================================================
//  srtf.cpp  –  Shortest Remaining Time First (preemptive) scheduling policy
//  C++14, STL only.
// ============================================================================
#include "srtf.hpp"

namespace mlsched {

void Srtf::on_job_arrival(Job& job, int64_t /*time*/) {
    waiting_set_.insert(&job);
}

void Srtf::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
    } else {
        waiting_set_.erase(&job);
    }
}

Job* Srtf::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    // 1. If current job has completed or finished, clear pointer
    if (current_ != nullptr && (current_->is_complete() || current_->finish_time != UNSET)) {
        current_ = nullptr;
    }

    // 2. Check if waiting_set_ has a job that preempts current_
    if (current_ != nullptr && !waiting_set_.empty()) {
        Job* top = *waiting_set_.begin();
        if (top->remaining < current_->remaining ||
            (top->remaining == current_->remaining && top->id < current_->id))
        {
            // Preemption: pop new runner, re-insert preempted job with updated remaining time
            waiting_set_.erase(waiting_set_.begin());
            waiting_set_.insert(current_);
            current_ = top;
        }
    }

    // 3. If no job is currently running, pick the most urgent waiting job
    if (current_ == nullptr && !waiting_set_.empty()) {
        current_ = *waiting_set_.begin();
        waiting_set_.erase(waiting_set_.begin());
    }

    // 4. Fallback: if waiting_set_ was empty but ready vector has jobs (e.g. direct test calls)
    if (current_ == nullptr && !ready.empty()) {
        for (Job* j : ready) {
            if (j != nullptr && j->finish_time == UNSET && !j->is_complete()) {
                waiting_set_.insert(j);
            }
        }
        if (!waiting_set_.empty()) {
            current_ = *waiting_set_.begin();
            waiting_set_.erase(waiting_set_.begin());
        }
    }

    return current_;
}

void Srtf::on_tick(int64_t /*time*/) {
    // SRTF preemption is evaluated per-tick via pick_next.
}

} // namespace mlsched
