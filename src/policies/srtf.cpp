// ============================================================================
//  srtf.cpp  –  Shortest Remaining Time First (preemptive) scheduling policy
//  C++14, STL only.
// ============================================================================
#include "srtf.hpp"

namespace mlsched {

Job* Srtf::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    if (ready.empty()) return nullptr;

    // Build min-heap over all available ready jobs keyed by remaining time.
    std::priority_queue<Job*, std::vector<Job*>, Compare> pq;
    for (Job* j : ready) {
        if (j != nullptr) {
            pq.push(j);
        }
    }

    if (pq.empty()) return nullptr;
    return pq.top();
}

void Srtf::on_tick(int64_t /*time*/) {
    // SRTF preemption is evaluated per-tick via pick_next; no internal timer needed.
}

} // namespace mlsched
