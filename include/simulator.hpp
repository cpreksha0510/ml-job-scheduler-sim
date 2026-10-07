#pragma once
// ============================================================================
//  simulator.hpp  –  Discrete-tick Simulator
//  C++17, STL only.
// ============================================================================
#include "job.hpp"
#include "scheduler.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace mlsched {

// ---------------------------------------------------------------------------
//  TickEvent  –  one entry in the per-tick timeline log
// ---------------------------------------------------------------------------
struct TickEvent {
    int64_t  tick;
    uint32_t job_id;     ///< ID of the job that ran, or 0 for idle
    bool     is_idle;
    bool     is_preempted;  ///< true if a different job was preempted this tick
    int64_t  remaining_after; ///< remaining burst of the running job after this tick
};

// ---------------------------------------------------------------------------
//  Simulator
// ---------------------------------------------------------------------------
class Simulator {
public:
    /// @param scheduler  Owning pointer to the policy to use (transferred).
    explicit Simulator(std::unique_ptr<Scheduler> scheduler);

    // Non-copyable, movable.
    Simulator(const Simulator&)            = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&)                 = default;
    Simulator& operator=(Simulator&&)      = default;

    // ── Job loading ──────────────────────────────────────────────────────────

    /// Add a single job to the pending (not-yet-arrived) pool.
    void add_job(Job job);

    /// Replace the entire pending pool.  Clears any previously added jobs.
    void set_jobs(std::vector<Job> jobs);

    // ── Simulation control ───────────────────────────────────────────────────

    /// Run until all jobs are complete (or @p max_ticks is reached).
    /// @return  Number of ticks actually simulated.
    int64_t run(int64_t max_ticks = 1'000'000);

    // ── Results ──────────────────────────────────────────────────────────────

    /// Per-tick timeline, populated after run().
    const std::vector<TickEvent>& timeline() const noexcept { return timeline_; }

    /// Completed jobs (have finish_time set), populated after run().
    const std::vector<Job>& completed_jobs() const noexcept { return completed_; }

    /// Jobs that did not complete before max_ticks.
    const std::vector<Job>& incomplete_jobs() const noexcept { return incomplete_; }

    // ── Optional tick callback (for live output) ──────────────────────────────
    using TickCallback = std::function<void(const TickEvent&, const Job*)>;
    void set_tick_callback(TickCallback cb) { tick_cb_ = std::move(cb); }

private:
    // ── Internal helpers ─────────────────────────────────────────────────────

    /// Admit all jobs whose arrival <= current_tick_ into the ready queue.
    void admit_arrivals();

    /// Execute one simulation tick.
    void tick();

    // ── State ─────────────────────────────────────────────────────────────────
    std::unique_ptr<Scheduler> scheduler_;

    std::vector<Job>   pending_;    ///< Jobs not yet arrived (sorted by arrival)
    std::vector<Job*>  ready_;      ///< Pointers into all_jobs_ (arrived, not complete)
    std::vector<Job>   all_jobs_;   ///< Owns all Job objects during simulation
    std::vector<Job>   completed_;  ///< Completed jobs (copied out)
    std::vector<Job>   incomplete_; ///< Incomplete jobs (copied out after run)

    Job*    running_  = nullptr;    ///< Currently running job (ptr into all_jobs_)
    int64_t current_tick_ = 0;

    std::vector<TickEvent> timeline_;
    TickCallback tick_cb_;
};

} // namespace mlsched
