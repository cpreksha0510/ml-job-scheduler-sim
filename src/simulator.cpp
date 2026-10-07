// ============================================================================
//  simulator.cpp  –  Simulator implementation
//  C++17, STL only.
// ============================================================================
#include "simulator.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace mlsched {

// ---------------------------------------------------------------------------
Simulator::Simulator(std::unique_ptr<Scheduler> scheduler)
    : scheduler_(std::move(scheduler))
{
    if (!scheduler_)
        throw std::invalid_argument("Simulator: scheduler must not be null");
}

// ---------------------------------------------------------------------------
void Simulator::add_job(Job job) {
    pending_.push_back(std::move(job));
}

// ---------------------------------------------------------------------------
void Simulator::set_jobs(std::vector<Job> jobs) {
    pending_ = std::move(jobs);
    all_jobs_.clear();
    ready_.clear();
    completed_.clear();
    incomplete_.clear();
    running_ = nullptr;
    current_tick_ = 0;
    timeline_.clear();
}

// ---------------------------------------------------------------------------
int64_t Simulator::run(int64_t max_ticks) {
    // -- Setup ----------------------------------------------------------------
    // Move pending into all_jobs_ (so pointers remain stable throughout).
    all_jobs_ = std::move(pending_);
    pending_.clear();

    // Sort by arrival time for efficient admission scan.
    std::sort(all_jobs_.begin(), all_jobs_.end(),
              [](const Job& a, const Job& b){ return a.arrival < b.arrival; });

    size_t admit_cursor = 0; // index into all_jobs_ up to which we've admitted

    // -- Tick loop ------------------------------------------------------------
    current_tick_ = 0;
    while (current_tick_ < max_ticks) {
        // 1. Admit newly arrived jobs.
        while (admit_cursor < all_jobs_.size() &&
               all_jobs_[admit_cursor].arrival <= current_tick_)
        {
            Job& j = all_jobs_[admit_cursor];
            scheduler_->on_job_arrival(j, current_tick_);
            ready_.push_back(&j);
            ++admit_cursor;
        }

        // 2. Ask the scheduler which job to run.
        Job* chosen = scheduler_->pick_next(ready_, current_tick_);

        // 3. Handle preemption.
        bool preempted = false;
        if (scheduler_->is_preemptive() && running_ != nullptr && chosen != running_) {
            // running_ is preempted – it stays in ready_ (already there).
            preempted = true;
            running_ = nullptr;
        }

        // 4. Execute one tick of the chosen job.
        if (chosen != nullptr) {
            // Set start / first_run_time on first execution.
            if (chosen->start_time == UNSET) {
                chosen->start_time = current_tick_;
                chosen->first_run_time = current_tick_;
            }

            // Remove from ready queue while running (for non-preemptive schedulers
            // this keeps the interface consistent; re-added if preempted).
            if (!scheduler_->is_preemptive()) {
                // For non-preemptive mode: chosen runs exclusively until complete.
                // We do NOT remove it from ready_ here; pick_next may manage that.
            }

            chosen->remaining -= 1;
            running_ = chosen;

            // Check completion.
            bool just_completed = chosen->is_complete();
            if (just_completed) {
                chosen->finish_time = current_tick_ + 1;
                // Remove from ready queue.
                ready_.erase(std::remove(ready_.begin(), ready_.end(), chosen),
                             ready_.end());
                scheduler_->on_job_complete(*chosen, current_tick_);
                running_ = nullptr;
            }

            // Record timeline event.
            TickEvent ev{current_tick_, chosen->id, false, preempted,
                         chosen->remaining};
            timeline_.push_back(ev);
            if (tick_cb_) tick_cb_(ev, chosen);

        } else {
            // Idle tick.
            TickEvent ev{current_tick_, 0, true, false, 0};
            timeline_.push_back(ev);
            if (tick_cb_) tick_cb_(ev, nullptr);
        }

        // 5. Notify scheduler.
        scheduler_->on_tick(current_tick_);

        ++current_tick_;

        // 6. Check if all jobs are done.
        bool any_pending  = admit_cursor < all_jobs_.size();
        bool any_running  = !ready_.empty();
        if (!any_pending && !any_running) break;
    }

    // -- Collect results ------------------------------------------------------
    for (auto& j : all_jobs_) {
        if (j.is_complete())
            completed_.push_back(j);
        else
            incomplete_.push_back(j);
    }

    // Restore pending_ to empty (all_jobs_ owns everything now).
    return current_tick_;
}

// ---------------------------------------------------------------------------
void Simulator::admit_arrivals() {
    // Not used directly (inlined in run()), kept for potential future use.
}

void Simulator::tick() {
    // Not used directly (inlined in run()), kept for potential future use.
}

} // namespace mlsched
