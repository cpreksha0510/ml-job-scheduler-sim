#pragma once
// ============================================================================
//  metrics.hpp  –  Metrics module for the ML Job Scheduler Simulator
//  C++14, STL only.
// ============================================================================
#include "job.hpp"
#include "simulator.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mlsched {

// ---------------------------------------------------------------------------
//  JobMetric  –  per-job evaluated metrics
// ---------------------------------------------------------------------------
struct JobMetric {
    uint32_t    job_id          = 0;
    std::string label;
    JobClass    job_class       = JobClass::Training;
    int64_t     arrival         = 0;
    int64_t     burst           = 0;
    int64_t     deadline        = UNSET;
    int64_t     start_time      = UNSET;
    int64_t     finish_time     = UNSET;
    int64_t     first_run_time  = UNSET;

    int64_t     waiting_time    = 0;      ///< turnaround - burst
    int64_t     turnaround_time = 0;      ///< finish - arrival
    int64_t     response_time   = 0;      ///< first_run - arrival

    bool        has_deadline    = false;
    bool        missed_deadline = false;  ///< has_deadline && finish > deadline
    bool        is_starved      = false;  ///< waiting_time > starvation_threshold
};

// ---------------------------------------------------------------------------
//  ClassMetrics  –  aggregate metrics for a specific JobClass
// ---------------------------------------------------------------------------
struct ClassMetrics {
    JobClass job_class           = JobClass::Training;
    size_t   job_count           = 0;
    double   avg_waiting_time    = 0.0;
    double   avg_turnaround_time = 0.0;
    double   avg_response_time   = 0.0;
    size_t   deadline_count      = 0;
    size_t   deadline_miss_count = 0;
    double   deadline_miss_rate  = 0.0;
    int64_t  max_waiting_time    = 0;
    size_t   starved_count       = 0;
    double   throughput          = 0.0;   ///< class completed jobs / overall makespan
};

// ---------------------------------------------------------------------------
//  AggregateMetrics  –  overall simulation metrics + per-class summaries
// ---------------------------------------------------------------------------
struct AggregateMetrics {
    std::string policy_name;
    size_t      total_jobs          = 0;
    int64_t     makespan            = 0;   ///< last finish time among completed jobs
    double      throughput          = 0.0; ///< total completed jobs / makespan

    double      avg_waiting_time    = 0.0;
    double      avg_turnaround_time = 0.0;
    double      avg_response_time   = 0.0;

    size_t      deadline_count      = 0;
    size_t      deadline_miss_count = 0;
    double      deadline_miss_rate  = 0.0;

    int64_t     max_waiting_time    = 0;
    size_t      starved_count       = 0;
    int64_t     starvation_threshold = 10;

    ClassMetrics training;
    ClassMetrics inference;
    ClassMetrics preprocessing;
};

// ---------------------------------------------------------------------------
//  Metrics  –  evaluator and CSV exporter
// ---------------------------------------------------------------------------
class Metrics {
public:
    Metrics() = default;

    /// Calculate metrics from a completed Simulator run.
    static Metrics calculate(const Simulator& sim,
                             const std::string& policy_name = "",
                             int64_t starvation_threshold = 10);

    const AggregateMetrics& aggregate() const noexcept { return agg_; }
    const AggregateMetrics& overall() const noexcept { return agg_; }

    const ClassMetrics& class_metrics(JobClass c) const;

    const std::vector<JobMetric>& job_metrics() const noexcept { return jobs_; }
    const JobMetric& job_metric(uint32_t id) const;

    const std::vector<ExecutionSegment>& segments() const noexcept { return segments_; }

    // ── CSV Exporters (creates output directory if missing) ───────────────────
    bool export_results_csv(const std::string& filepath = "results/results.csv") const;
    bool export_per_job_csv(const std::string& filepath = "results/per_job.csv") const;
    bool export_timeline_csv(const std::string& filepath = "results/timeline.csv") const;
    bool export_all(const std::string& output_dir = "results") const;

private:
    AggregateMetrics agg_;
    std::vector<JobMetric> jobs_;
    std::vector<ExecutionSegment> segments_;
};

} // namespace mlsched
