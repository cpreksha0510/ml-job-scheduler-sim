#pragma once
// ============================================================================
//  hybrid.hpp  –  ML-Workload-Aware Hybrid Scheduling Policy
//
//  Three-tier hybrid scheduler:
//    - Tier 0: Inference jobs (EDF, preemptive, no quantum, never demoted).
//    - Tier 1: Preprocessing jobs (FIFO, quantum = tier1_quantum, demotes to tier 2).
//    - Tier 2: Training jobs (FIFO, quantum = tier2_quantum, round-robin in tier 2).
//
//  Features:
//    - Strict tier priority (Tier 0 > Tier 1 > Tier 2).
//    - Preemptive on higher-tier arrival (preempted job stays in same tier).
//    - Aging: Tier 2 jobs promoted to Tier 1 upon reaching aging_threshold.
//    - Reserved slot: Every tick t where t % K == K-1 reserves the tick for Tier 2.
//
//  Rules (SPEC.md / AGENT_RULES.md):
//    - Inherits from mlsched::Scheduler publicly.
//    - Never touches Simulator internals.
//    - C++14, STL only.
// ============================================================================
#include "scheduler.hpp"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace mlsched {

struct HybridConfig {
    int64_t tier1_quantum   = 8;
    int64_t tier2_quantum   = 32;
    int64_t aging_threshold = 100;
    int64_t reserve_period  = 10; // K (0 disables reserved slot)
};

class HybridScheduler final : public Scheduler {
public:
    explicit HybridScheduler(const HybridConfig& config = HybridConfig{});

    const char* name() const noexcept override { return "Hybrid"; }
    bool is_preemptive() const noexcept override { return true; }

    Job* pick_next(std::vector<Job*>& ready, int64_t time) override;
    void on_tick(int64_t time) override;
    void on_job_arrival(Job& job, int64_t time) override;
    void on_job_complete(Job& job, int64_t time) override;

    const HybridConfig& config() const noexcept { return config_; }

private:
    // Helper to determine if job 'a' has higher Tier 0 priority than 'b'
    static bool tier0_more_urgent(const Job* a, const Job* b) noexcept;

    HybridConfig config_;

    Job*    current_      = nullptr;
    int     current_tier_ = -1;
    int64_t ticks_used_   = 0;

    std::vector<Job*>       tier0_;      ///< Inference jobs (EDF)
    std::deque<Job*>        tier1_;      ///< Preprocessing / promoted jobs (FIFO)
    std::deque<Job*>        tier2_;      ///< Training / demoted jobs (FIFO)
    std::vector<Job*>       new_arrivals_;
    std::unordered_map<uint32_t, int64_t> tier2_wait_time_;
};

} // namespace mlsched
