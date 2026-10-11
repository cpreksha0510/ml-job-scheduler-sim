// ============================================================================
//  edf.cpp  –  Earliest Deadline First (preemptive) scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "edf.hpp"

namespace mlsched {

void Edf::on_job_arrival(Job& job, int64_t /*time*/) {
    ready_set_.insert(&job);
}

void Edf::on_job_complete(Job& job, int64_t /*time*/) {
    ready_set_.erase(&job);
}

Job* Edf::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    if (ready_set_.empty() && !ready.empty()) {
        // Fallback if ready vector has jobs not tracked by arrival hooks (e.g. direct test calls)
        for (Job* j : ready) {
            if (j != nullptr && j->finish_time == UNSET && !j->is_complete()) {
                ready_set_.insert(j);
            }
        }
    }

    if (ready_set_.empty()) return nullptr;
    return *ready_set_.begin();
}

void Edf::on_tick(int64_t /*time*/) {
    // Dynamic preemption evaluated per tick via pick_next.
}

} // namespace mlsched
