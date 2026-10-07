#pragma once
// ============================================================================
//  workload.hpp  –  Synthetic workload generator for ML Job Scheduler Simulator
//  C++14, STL only.
// ============================================================================
#include "job.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace mlsched {

// ---------------------------------------------------------------------------
//  WorkloadConfig  –  parameters controlling synthetic workload generation
// ---------------------------------------------------------------------------
struct WorkloadConfig {
    uint32_t seed    = 42;
    int64_t  horizon = 1000; ///< Maximum arrival tick threshold (simulation horizon)

    // Inference: Poisson arrival process, short burst, high priority, tight deadline
    double  infer_rate      = 0.1;   ///< Mean arrivals per tick
    int64_t infer_burst_min = 1;
    int64_t infer_burst_max = 4;
    int32_t infer_priority  = 10;
    double  infer_slack     = 1.5;   ///< deadline = arrival + ceil(burst * slack)
    int64_t infer_mem_min   = 256;
    int64_t infer_mem_max   = 512;

    // Training: Sparse arrivals, long bursts, low priority, high memory, optional deadline
    double  train_rate      = 0.005; ///< Mean arrivals per tick
    int64_t train_burst_min = 40;
    int64_t train_burst_max = 80;
    int32_t train_priority  = 2;
    double  train_slack     = -1.0;  ///< <= 0 means UNSET (-1)
    int64_t train_mem_min   = 4096;
    int64_t train_mem_max   = 16384;

    // Preprocessing: Batchy arrivals, medium bursts, medium priority, loose deadline
    double  preproc_rate       = 0.01; ///< Mean batch arrivals per tick
    int32_t preproc_batch_size = 4;    ///< Jobs per batch
    int64_t preproc_burst_min  = 10;
    int64_t preproc_burst_max  = 25;
    int32_t preproc_priority   = 5;
    double  preproc_slack      = 3.0;
    int64_t preproc_mem_min    = 1024;
    int64_t preproc_mem_max    = 2048;

    /// Load config from key=value file (skips blank lines and lines starting with #).
    static WorkloadConfig load_from_file(const std::string& path);
};

// ---------------------------------------------------------------------------
//  WorkloadSummary  –  summary stats of generated jobs
// ---------------------------------------------------------------------------
struct WorkloadSummary {
    size_t  total_jobs        = 0;
    size_t  infer_jobs        = 0;
    size_t  train_jobs        = 0;
    size_t  preproc_jobs      = 0;
    int64_t total_demand      = 0; ///< sum of burst times across all jobs
    int64_t infer_demand      = 0;
    int64_t train_demand      = 0;
    int64_t preproc_demand    = 0;
    int64_t horizon           = 0;
    double  demand_ratio      = 0.0; ///< total_demand / horizon
};

// ---------------------------------------------------------------------------
//  WorkloadGenerator
// ---------------------------------------------------------------------------
class WorkloadGenerator {
public:
    explicit WorkloadGenerator(WorkloadConfig config);

    /// Generate jobs sorted by arrival, with sequentially assigned IDs 0..n-1.
    std::vector<Job> generate();

    /// Calculate summary of a workload against horizon.
    static WorkloadSummary summarize(const std::vector<Job>& jobs, int64_t horizon);

private:
    WorkloadConfig config_;
    std::mt19937   rng_;

    // Deterministic random helpers (portable across compilers)
    double  uniform_real();
    int64_t uniform_int(int64_t low, int64_t high);
    double  sample_exponential(double rate);
};

// ---------------------------------------------------------------------------
//  CSV Serializer
// ---------------------------------------------------------------------------
/// Write jobs to a CSV matching csv_loader.hpp format (id,class,arrival,burst,...).
bool save_jobs_to_csv(const std::string& path, const std::vector<Job>& jobs);

} // namespace mlsched
