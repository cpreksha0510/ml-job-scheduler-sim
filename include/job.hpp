#pragma once
// ============================================================================
//  job.hpp  –  Core Job struct for the ML Job Scheduler Simulator
//  C++17, STL only. Do NOT modify without reading AGENT_RULES.md first.
// ============================================================================
#include <cstdint>
#include <string>

namespace mlsched {

/// Three workload classes described in SPEC.md §Design.
enum class JobClass : uint8_t {
    Training     = 0,  ///< Long, throughput-bound, lower priority
    Inference    = 1,  ///< Short, latency-sensitive, hard deadline
    Preprocessing = 2  ///< Batch, medium burst, no hard deadline
};

/// Returns a short label string for display / CSV.
inline const char* to_string(JobClass c) noexcept {
    switch (c) {
        case JobClass::Training:      return "TRAIN";
        case JobClass::Inference:     return "INFER";
        case JobClass::Preprocessing: return "PREPROC";
    }
    return "UNKNOWN";
}

// Sentinel value: field is "not yet set".
static constexpr int64_t UNSET = -1;

/// Core job descriptor.  All time fields are in integer ticks.
struct Job {
    // ── Identity ──────────────────────────────────────────────────────────────
    uint32_t  id;          ///< Unique job ID (assigned by generator / CSV)
    JobClass  job_class;   ///< Training | Inference | Preprocessing
    std::string label;     ///< Human-readable name (optional, defaults to "J<id>")

    // ── Scheduling parameters ─────────────────────────────────────────────────
    int64_t   arrival;     ///< Tick at which the job enters the system
    int64_t   burst;       ///< Total CPU ticks required (original)
    int64_t   remaining;   ///< Remaining CPU ticks (decremented by simulator)
    int32_t   priority;    ///< Static priority (higher == more urgent for some policies)
    int64_t   deadline;    ///< Absolute deadline tick (UNSET = no deadline)
    int64_t   mem_req;     ///< Memory requirement in MB (resource-awareness hook)

    // ── Metrics (filled by Simulator, read by MetricsCollector) ───────────────
    int64_t   start_time;      ///< Tick when the job first gets CPU (response time base)
    int64_t   finish_time;     ///< Tick when remaining reaches 0
    int64_t   first_run_time;  ///< Same as start_time; kept separate for clarity

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool is_complete()    const noexcept { return remaining <= 0; }
    bool has_deadline()   const noexcept { return deadline != UNSET; }
    bool missed_deadline() const noexcept {
        return has_deadline() && finish_time != UNSET && finish_time > deadline;
    }

    int64_t waiting_time()    const noexcept {
        if (start_time == UNSET) return UNSET;
        return start_time - arrival;
    }
    int64_t turnaround_time() const noexcept {
        if (finish_time == UNSET) return UNSET;
        return finish_time - arrival;
    }
    int64_t response_time()   const noexcept {
        if (first_run_time == UNSET) return UNSET;
        return first_run_time - arrival;
    }

    /// Factory: construct with mandatory fields; metric fields default to UNSET.
    static Job make(uint32_t id_, JobClass cls, int64_t arrival_,
                    int64_t burst_, int32_t priority_ = 0,
                    int64_t deadline_ = UNSET, int64_t mem_req_ = 0,
                    std::string label_ = "")
    {
        return Job{
            id_,
            cls,
            label_.empty() ? "J" + std::to_string(id_) : std::move(label_),
            arrival_, burst_, burst_,   // remaining = burst initially
            priority_, deadline_, mem_req_,
            UNSET, UNSET, UNSET          // metric fields
        };
    }
};

} // namespace mlsched
