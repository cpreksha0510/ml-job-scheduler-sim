// ============================================================================
//  edf.cpp  –  Earliest Deadline First (preemptive) scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "edf.hpp"

namespace mlsched {

Job* Edf::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    if (ready.empty()) return nullptr;

    // Build min-heap over all available ready jobs.
    // Jobs that have already missed their deadline are NOT dropped or skipped;
    // they remain in the ready queue and keep running in deadline order.
    std::priority_queue<Job*, std::vector<Job*>, Compare> pq;
    for (Job* j : ready) {
        if (j != nullptr) {
            pq.push(j);
        }
    }

    if (pq.empty()) return nullptr;
    return pq.top();
}

void Edf::on_tick(int64_t /*time*/) {
    // Dynamic preemption evaluated per tick via pick_next.
}

} // namespace mlsched
