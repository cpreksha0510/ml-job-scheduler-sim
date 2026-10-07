// ============================================================================
//  sjf.cpp  –  Shortest Job First (non-preemptive) scheduling policy
//  C++14, STL only.
// ============================================================================
#include "sjf.hpp"

namespace mlsched {

Job* Sjf::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    // Non-preemptive: if a job is currently running and not complete, continue it.
    if (current_ != nullptr && !current_->is_complete()) {
        return current_;
    }
    current_ = nullptr;

    if (ready.empty()) return nullptr;

    // Build min-heap over all available ready jobs.
    std::priority_queue<Job*, std::vector<Job*>, Compare> pq;
    for (Job* j : ready) {
        if (j != nullptr) {
            pq.push(j);
        }
    }

    if (pq.empty()) return nullptr;

    current_ = pq.top();
    return current_;
}

void Sjf::on_tick(int64_t /*time*/) {
    // Non-preemptive SJF requires no per-tick state update.
}

void Sjf::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
    }
}

} // namespace mlsched
