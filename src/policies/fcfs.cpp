// ============================================================================
//  fcfs.cpp  –  First-Come First-Served scheduling policy implementation
//  C++14, STL only.
// ============================================================================
#include "fcfs.hpp"

#include <algorithm>  // std::min_element

namespace mlsched {

// ---------------------------------------------------------------------------
Job* Fcfs::pick_next(std::vector<Job*>& ready, int64_t /*time*/) {
    if (ready.empty()) return nullptr;

    // Return the job that arrived earliest (ties broken by lower id for
    // determinism, matching textbook FCFS behaviour).
    return *std::min_element(ready.begin(), ready.end(),
        [](const Job* a, const Job* b) {
            if (a->arrival != b->arrival) return a->arrival < b->arrival;
            return a->id < b->id;
        });
}

// ---------------------------------------------------------------------------
void Fcfs::on_tick(int64_t /*time*/) {
    // Plain FCFS has no per-tick bookkeeping.
}

} // namespace mlsched
