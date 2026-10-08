// ============================================================================
//  hybrid.cpp  –  ML-Workload-Aware Hybrid Scheduling Policy implementation
//  C++14, STL only.
// ============================================================================
#include "hybrid.hpp"

#include <algorithm>

namespace mlsched {

HybridScheduler::HybridScheduler(const HybridConfig& config)
    : config_(config)
{
    if (config_.tier1_quantum <= 0)   config_.tier1_quantum = 8;
    if (config_.tier2_quantum <= 0)   config_.tier2_quantum = 32;
    if (config_.aging_threshold < 0)  config_.aging_threshold = 0;
    if (config_.reserve_period < 0)   config_.reserve_period = 0;
}

bool HybridScheduler::tier0_more_urgent(const Job* a, const Job* b) noexcept {
    bool a_has = a->has_deadline();
    bool b_has = b->has_deadline();

    if (!a_has && !b_has) return a->id < b->id;
    if (!a_has && b_has)  return false;
    if (a_has && !b_has)  return true;

    if (a->deadline != b->deadline) {
        return a->deadline < b->deadline;
    }
    return a->id < b->id;
}

void HybridScheduler::on_job_arrival(Job& job, int64_t /*time*/) {
    new_arrivals_.push_back(&job);
}

void HybridScheduler::on_job_complete(Job& job, int64_t /*time*/) {
    if (current_ == &job) {
        current_ = nullptr;
        current_tier_ = -1;
        ticks_used_ = 0;
    }
    tier2_wait_time_.erase(job.id);
}

void HybridScheduler::on_tick(int64_t /*time*/) {
    if (current_ != nullptr) {
        ++ticks_used_;
        if (current_tier_ == 2) {
            tier2_wait_time_[current_->id] = 0;
        }
    }

    // Update wait counters for jobs waiting in tier 2 (ready but not running)
    for (Job* j : tier2_) {
        tier2_wait_time_[j->id] += 1;
    }

    // Check aging promotions: promote starved Tier 2 jobs to tail of Tier 1
    if (config_.aging_threshold > 0) {
        std::deque<Job*> remaining_tier2;
        for (Job* j : tier2_) {
            if (tier2_wait_time_[j->id] >= config_.aging_threshold) {
                tier1_.push_back(j);
                tier2_wait_time_.erase(j->id);
            } else {
                remaining_tier2.push_back(j);
            }
        }
        tier2_ = std::move(remaining_tier2);
    }
}

Job* HybridScheduler::pick_next(std::vector<Job*>& ready, int64_t time) {
    // 1. Process new arrivals first:
    //    "Tie at the same tick: arrivals are enqueued before a demoted or preempted job is requeued."
    if (!new_arrivals_.empty()) {
        std::sort(new_arrivals_.begin(), new_arrivals_.end(),
                  [](const Job* a, const Job* b) { return a->id < b->id; });
        for (Job* j : new_arrivals_) {
            switch (j->job_class) {
                case JobClass::Inference:
                    tier0_.push_back(j);
                    break;
                case JobClass::Preprocessing:
                    tier1_.push_back(j);
                    break;
                case JobClass::Training:
                    tier2_.push_back(j);
                    tier2_wait_time_[j->id] = 0;
                    break;
                default:
                    // Unknown job class routes to tier 1
                    tier1_.push_back(j);
                    break;
            }
        }
        new_arrivals_.clear();
    }

    // 2. Quantum expiration for running job (if not already completed):
    if (current_ != nullptr && current_->finish_time != UNSET) {
        current_ = nullptr;
        current_tier_ = -1;
        ticks_used_ = 0;
    } else if (current_ != nullptr) {
        if (current_tier_ == 1 && ticks_used_ >= config_.tier1_quantum) {
            // Tier 1 job demoted to tail of Tier 2
            tier2_.push_back(current_);
            tier2_wait_time_[current_->id] = 0;
            current_ = nullptr;
            current_tier_ = -1;
            ticks_used_ = 0;
        } else if (current_tier_ == 2 && ticks_used_ >= config_.tier2_quantum) {
            // Tier 2 job requeued to tail of Tier 2
            tier2_.push_back(current_);
            tier2_wait_time_[current_->id] = 0;
            current_ = nullptr;
            current_tier_ = -1;
            ticks_used_ = 0;
        }
        // Tier 0 jobs have no quantum and are never demoted
    }

    // 3. Check reserved slot for Tier 2:
    //    "if reserve_period K > 0, every tick t where t % K == K-1 and tier 2 is non-empty
    //     goes to the head of tier 2, even if tier 0 or 1 has work."
    bool is_reserved_tick = false;
    if (config_.reserve_period > 0 &&
        (time % config_.reserve_period == config_.reserve_period - 1))
    {
        if (!tier2_.empty() || (current_ != nullptr && current_tier_ == 2)) {
            is_reserved_tick = true;
        }
    }

    if (is_reserved_tick) {
        if (current_ != nullptr && current_tier_ < 2) {
            // Running job displaced by reserved tick is treated as preempted
            if (current_tier_ == 0) {
                tier0_.push_back(current_);
            } else if (current_tier_ == 1) {
                tier1_.push_back(current_);
            }
            current_ = nullptr;
            current_tier_ = -1;
            ticks_used_ = 0;
        }

        if (current_ == nullptr && !tier2_.empty()) {
            current_ = tier2_.front();
            tier2_.pop_front();
            current_tier_ = 2;
            ticks_used_ = 0;
        }
        return current_;
    }

    // 4. Normal tick: Preemption checks for running job
    if (current_ != nullptr) {
        if (current_tier_ == 2) {
            // Preempted if Tier 0 or Tier 1 has work
            if (!tier0_.empty() || !tier1_.empty()) {
                tier2_.push_back(current_);
                tier2_wait_time_[current_->id] = 0;
                current_ = nullptr;
                current_tier_ = -1;
                ticks_used_ = 0;
            }
        } else if (current_tier_ == 1) {
            // Preempted if Tier 0 has work
            if (!tier0_.empty()) {
                tier1_.push_back(current_);
                current_ = nullptr;
                current_tier_ = -1;
                ticks_used_ = 0;
            }
        } else if (current_tier_ == 0) {
            // Preempted if Tier 0 has a job with an earlier deadline
            bool higher_urgency_found = false;
            for (Job* j : tier0_) {
                if (tier0_more_urgent(j, current_)) {
                    higher_urgency_found = true;
                    break;
                }
            }
            if (higher_urgency_found) {
                tier0_.push_back(current_);
                current_ = nullptr;
                current_tier_ = -1;
                ticks_used_ = 0;
            }
        }
    }

    // 5. Dispatch next job (Tier 0 > Tier 1 > Tier 2)
    if (current_ == nullptr) {
        if (!tier0_.empty()) {
            auto best_it = std::min_element(tier0_.begin(), tier0_.end(),
                [](const Job* a, const Job* b) {
                    return tier0_more_urgent(a, b);
                });
            current_ = *best_it;
            tier0_.erase(best_it);
            current_tier_ = 0;
            ticks_used_ = 0;
        } else if (!tier1_.empty()) {
            current_ = tier1_.front();
            tier1_.pop_front();
            current_tier_ = 1;
            ticks_used_ = 0;
        } else if (!tier2_.empty()) {
            current_ = tier2_.front();
            tier2_.pop_front();
            current_tier_ = 2;
            ticks_used_ = 0;
        }
    }

    // 6. Fallback: if internal queues were empty but ready vector has jobs
    if (current_ == nullptr && !ready.empty()) {
        for (Job* j : ready) {
            if (j != nullptr && j->finish_time == UNSET) {
                if (j->job_class == JobClass::Inference) {
                    tier0_.push_back(j);
                } else if (j->job_class == JobClass::Preprocessing) {
                    tier1_.push_back(j);
                } else {
                    tier2_.push_back(j);
                }
            }
        }
        if (!tier0_.empty()) {
            current_ = tier0_.front();
            tier0_.erase(tier0_.begin());
            current_tier_ = 0;
            ticks_used_ = 0;
        } else if (!tier1_.empty()) {
            current_ = tier1_.front();
            tier1_.pop_front();
            current_tier_ = 1;
            ticks_used_ = 0;
        } else if (!tier2_.empty()) {
            current_ = tier2_.front();
            tier2_.pop_front();
            current_tier_ = 2;
            ticks_used_ = 0;
        }
    }

    return current_;
}

} // namespace mlsched
