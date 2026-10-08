// ============================================================================
//  benchmark.cpp  --  Scaling benchmark suite for ML Job Scheduler Simulator
//  C++14, STL only. Built with -O2 optimization for benchmark target.
// ============================================================================

#include "simulator.hpp"
#include "metrics.hpp"
#include "workload.hpp"

#include "policies/fcfs.hpp"
#include "policies/sjf.hpp"
#include "policies/srtf.hpp"
#include "policies/rr.hpp"
#include "policies/mlfq.hpp"
#include "policies/edf.hpp"
#include "policies/hybrid.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Peak memory helper (Windows Working Set size)
// ---------------------------------------------------------------------------
double get_peak_memory_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

// ---------------------------------------------------------------------------
// 64-bit FNV-1a hash for string checksum
// ---------------------------------------------------------------------------
uint64_t fnv1a_hash(const std::string& str) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// Scheduler decorator to track peak ready queue length non-intrusively
// ---------------------------------------------------------------------------
class MonitoredScheduler : public mlsched::Scheduler {
public:
    explicit MonitoredScheduler(std::unique_ptr<mlsched::Scheduler> inner)
        : inner_(std::move(inner)) {}

    mlsched::Job* pick_next(std::vector<mlsched::Job*>& ready, int64_t time) override {
        if (ready.size() > peak_ready_) {
            peak_ready_ = ready.size();
        }
        return inner_->pick_next(ready, time);
    }

    void on_tick(int64_t time) override {
        inner_->on_tick(time);
    }

    void on_job_arrival(mlsched::Job& job, int64_t time) override {
        inner_->on_job_arrival(job, time);
    }

    void on_job_complete(mlsched::Job& job, int64_t time) override {
        inner_->on_job_complete(job, time);
    }

    bool is_preemptive() const noexcept override {
        return inner_->is_preemptive();
    }

    const char* name() const noexcept override {
        return inner_->name();
    }

    size_t peak_ready() const noexcept { return peak_ready_; }

private:
    std::unique_ptr<mlsched::Scheduler> inner_;
    size_t peak_ready_ = 0;
};

// ---------------------------------------------------------------------------
// Policy Factory
// ---------------------------------------------------------------------------
std::unique_ptr<mlsched::Scheduler> create_policy(const std::string& name) {
    if (name == "FCFS") return std::make_unique<mlsched::Fcfs>();
    if (name == "SJF")  return std::make_unique<mlsched::Sjf>();
    if (name == "SRTF") return std::make_unique<mlsched::Srtf>();
    if (name == "RR")   return std::make_unique<mlsched::RoundRobin>(3); // Small quantum = 3
    if (name == "MLFQ") return std::make_unique<mlsched::Mlfq>(2, 4, 8, 0); // Default, boost off
    if (name == "EDF")  return std::make_unique<mlsched::Edf>();
    if (name == "Hybrid") {
        mlsched::HybridConfig cfg;
        cfg.tier1_quantum   = 8;
        cfg.tier2_quantum   = 32;
        cfg.aging_threshold = 100;
        cfg.reserve_period  = 10;
        return std::make_unique<mlsched::HybridScheduler>(cfg);
    }
    throw std::runtime_error("Unknown policy name: " + name);
}

// ---------------------------------------------------------------------------
// Benchmark Run Result Record
// ---------------------------------------------------------------------------
struct BenchmarkRecord {
    std::string workload;
    int64_t     target_jobs;
    size_t      actual_jobs;
    std::string policy;
    double      rep_secs[3];
    double      median_sec;
    double      jobs_per_sec;
    int64_t     simulated_ticks;
    int64_t     busy_ticks;
    size_t      peak_ready_queue;
    double      peak_memory_mb;
    bool        sanity_passed;
    std::string status;
};

// ---------------------------------------------------------------------------
// Single Simulation Run with Timeout Watchdog
// ---------------------------------------------------------------------------
struct SingleRunResult {
    double  seconds          = 0.0;
    int64_t simulated_ticks  = 0;
    int64_t busy_ticks       = 0;
    size_t  peak_ready_queue = 0;
    bool    sanity_passed    = false;
    bool    timed_out        = false;
    std::string per_job_csv;
};

SingleRunResult run_single_simulation(const std::string& policy_name,
                                      const std::vector<mlsched::Job>& jobs,
                                      int64_t total_burst,
                                      bool capture_csv = false)
{
    SingleRunResult result;
    auto monitored = std::make_unique<MonitoredScheduler>(create_policy(policy_name));
    MonitoredScheduler* mon_ptr = monitored.get();

    mlsched::Simulator sim(std::move(monitored));
    sim.set_jobs(jobs);

    int64_t tick_check_counter = 0;
    auto t_start = std::chrono::steady_clock::now();

    sim.set_tick_callback([&](const mlsched::TickEvent&, const mlsched::Job*) {
        // Check 120s timeout periodically (every 4096 ticks) to avoid clock syscall overhead
        if ((++tick_check_counter & 4095) == 0) {
            auto t_cur = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(t_cur - t_start).count();
            if (elapsed > 120.0) {
                throw std::runtime_error("TIMEOUT");
            }
        }
    });

    try {
        result.simulated_ticks = sim.run(100'000'000);
        auto t_end = std::chrono::steady_clock::now();
        result.seconds = std::chrono::duration<double>(t_end - t_start).count();
        result.peak_ready_queue = mon_ptr->peak_ready();

        // Calculate busy ticks
        int64_t busy = 0;
        for (const auto& ev : sim.timeline()) {
            if (!ev.is_idle) ++busy;
        }
        result.busy_ticks = busy;

        // Sanity check: all jobs complete and busy ticks == sum of bursts
        bool all_complete = (sim.completed_jobs().size() == jobs.size());
        bool bursts_match = (busy == total_burst);
        result.sanity_passed = (all_complete && bursts_match);

        if (capture_csv) {
            auto metrics = mlsched::Metrics::calculate(sim, policy_name, 10);
            std::ostringstream ss;
            // Capture formatted per_job table to string for hash verification
            ss << "id,class,arrival,burst,deadline,finish,waiting,turnaround,response\n";
            for (const auto& jm : metrics.job_metrics()) {
                ss << jm.job_id << "," << static_cast<int>(jm.job_class) << ","
                   << jm.arrival << "," << jm.burst << "," << jm.deadline << ","
                   << jm.finish_time << "," << jm.waiting_time << ","
                   << jm.turnaround_time << "," << jm.response_time << "\n";
            }
            result.per_job_csv = ss.str();
        }

    } catch (const std::exception& e) {
        if (std::string(e.what()) == "TIMEOUT") {
            result.timed_out = true;
            result.seconds = 120.0;
        } else {
            throw;
        }
    }

    return result;
}

} // namespace

int main() {
    std::cout << "============================================================\n";
    std::cout << " ML Job Scheduler Simulator -- Scaling Benchmark (-O2)\n";
    std::cout << "============================================================\n\n";

    // 1. Workload generation
    std::cout << "[Step 1] Generating synthetic workloads (seed 1)...\n";
    auto cfg_med = mlsched::WorkloadConfig::load_from_file("data/workload_medium.cfg");
    cfg_med.seed = 1;
    double rate_med = cfg_med.infer_rate + cfg_med.train_rate +
                      cfg_med.preproc_rate * cfg_med.preproc_batch_size;

    auto cfg_ov = mlsched::WorkloadConfig::load_from_file("data/workload_overloaded.cfg");
    cfg_ov.seed = 1;
    double rate_ov = cfg_ov.infer_rate + cfg_ov.train_rate +
                     cfg_ov.preproc_rate * cfg_ov.preproc_batch_size;

    struct WorkloadDef {
        std::string workload_name;
        int64_t     target_jobs;
        int64_t     horizon;
        mlsched::WorkloadConfig config;
        std::vector<mlsched::Job> jobs;
        int64_t     total_burst = 0;
    };

    std::vector<WorkloadDef> workloads = {
        {"medium",     1000,   static_cast<int64_t>(1000   / rate_med), cfg_med, {}, 0},
        {"medium",     10000,  static_cast<int64_t>(10000  / rate_med), cfg_med, {}, 0},
        {"medium",     100000, static_cast<int64_t>(100000 / rate_med), cfg_med, {}, 0},
        {"overloaded", 10000,  static_cast<int64_t>(10000  / rate_ov),  cfg_ov,  {}, 0}
    };

    for (auto& w : workloads) {
        w.config.horizon = w.horizon;
        mlsched::WorkloadGenerator gen(w.config);
        w.jobs = gen.generate();
        w.total_burst = 0;
        for (const auto& j : w.jobs) {
            w.total_burst += j.burst;
        }
        std::cout << "  - " << w.workload_name << " (target " << w.target_jobs
                  << ", horizon " << w.horizon << "): generated "
                  << w.jobs.size() << " actual jobs (total demand: "
                  << w.total_burst << " ticks)\n";
    }
    std::cout << "\n";

    // 2. Policies
    std::vector<std::string> policies = {
        "FCFS", "SJF", "SRTF", "RR", "MLFQ", "EDF", "Hybrid"
    };

    std::vector<BenchmarkRecord> records;

    // 3. Execution loop
    std::cout << "[Step 2] Executing policy benchmarks (3 repetitions, median recorded)...\n";
    std::cout << "----------------------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(12) << "Workload"
              << std::setw(8)  << "Jobs"
              << std::setw(10) << "Policy"
              << std::setw(12) << "Median (s)"
              << std::setw(14) << "Jobs/sec"
              << std::setw(12) << "Peak Ready"
              << std::setw(12) << "Memory (MB)"
              << std::setw(10) << "Status" << "\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    for (const auto& w : workloads) {
        for (const auto& pol_name : policies) {
            BenchmarkRecord rec;
            rec.workload     = w.workload_name;
            rec.target_jobs  = w.target_jobs;
            rec.actual_jobs  = w.jobs.size();
            rec.policy       = pol_name;

            std::vector<double> rep_times;
            SingleRunResult last_res;

            for (int r = 0; r < 3; ++r) {
                last_res = run_single_simulation(pol_name, w.jobs, w.total_burst, false);
                rec.rep_secs[r] = last_res.seconds;
                if (last_res.timed_out) {
                    break;
                }
                rep_times.push_back(last_res.seconds);
            }

            if (last_res.timed_out) {
                rec.median_sec        = 120.0;
                rec.jobs_per_sec      = 0.0;
                rec.simulated_ticks   = last_res.simulated_ticks;
                rec.busy_ticks        = last_res.busy_ticks;
                rec.peak_ready_queue  = last_res.peak_ready_queue;
                rec.peak_memory_mb    = get_peak_memory_mb();
                rec.sanity_passed     = false;
                rec.status            = "TIMEOUT";
            } else {
                std::sort(rep_times.begin(), rep_times.end());
                rec.median_sec        = rep_times[1]; // Median of 3
                rec.jobs_per_sec      = (rec.median_sec > 0.0) ? (static_cast<double>(rec.actual_jobs) / rec.median_sec) : 0.0;
                rec.simulated_ticks   = last_res.simulated_ticks;
                rec.busy_ticks        = last_res.busy_ticks;
                rec.peak_ready_queue  = last_res.peak_ready_queue;
                rec.peak_memory_mb    = get_peak_memory_mb();
                rec.sanity_passed     = last_res.sanity_passed;
                rec.status            = rec.sanity_passed ? "PASS" : "SANITY_FAIL";
            }

            records.push_back(rec);

            std::cout << std::left << std::setw(12) << rec.workload
                      << std::setw(8)  << rec.actual_jobs
                      << std::setw(10) << rec.policy
                      << std::setw(12) << std::fixed << std::setprecision(5) << rec.median_sec
                      << std::setw(14) << std::fixed << std::setprecision(1) << rec.jobs_per_sec
                      << std::setw(12) << rec.peak_ready_queue
                      << std::setw(12) << std::fixed << std::setprecision(2) << rec.peak_memory_mb
                      << std::setw(10) << rec.status << "\n";
        }
    }
    std::cout << "----------------------------------------------------------------------------------------\n\n";

    // 4. Scaling ratio check: time(100k) / time(10k)
    std::cout << "[Step 3] Scaling Check: time(100k) / time(10k) on Medium Workload\n";
    std::cout << "--------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(10) << "Policy"
              << std::setw(14) << "Time 10k (s)"
              << std::setw(14) << "Time 100k (s)"
              << std::setw(14) << "Ratio (100k/10k)"
              << std::setw(16) << "Assessment" << "\n";
    std::cout << "--------------------------------------------------------------------\n";

    for (const auto& pol_name : policies) {
        double t_10k  = -1.0;
        double t_100k = -1.0;
        for (const auto& r : records) {
            if (r.workload == "medium" && r.policy == pol_name) {
                if (r.target_jobs == 10000)  t_10k  = r.median_sec;
                if (r.target_jobs == 100000) t_100k = r.median_sec;
            }
        }

        std::string assess = "NORMAL (~10-12)";
        double ratio = 0.0;
        if (t_10k > 0.0 && t_100k > 0.0) {
            ratio = t_100k / t_10k;
            if (ratio > 15.0) {
                assess = "SUSPICIOUS (>15)";
            }
        } else {
            assess = "INVALID";
        }

        std::cout << std::left << std::setw(10) << pol_name
                  << std::setw(14) << std::fixed << std::setprecision(5) << t_10k
                  << std::setw(14) << std::fixed << std::setprecision(5) << t_100k
                  << std::setw(14) << std::fixed << std::setprecision(2) << ratio
                  << std::setw(16) << assess << "\n";
    }
    std::cout << "--------------------------------------------------------------------\n\n";

    // 5. Determinism check: Hybrid on 100k run twice
    std::cout << "[Step 4] Determinism Verification at Scale (Hybrid on 100k workload)...\n";
    const auto& w_100k = workloads[2]; // medium 100k
    auto run1 = run_single_simulation("Hybrid", w_100k.jobs, w_100k.total_burst, true);
    auto run2 = run_single_simulation("Hybrid", w_100k.jobs, w_100k.total_burst, true);

    uint64_t hash1 = fnv1a_hash(run1.per_job_csv);
    uint64_t hash2 = fnv1a_hash(run2.per_job_csv);

    std::cout << "  Run 1 per_job hash: 0x" << std::hex << hash1 << std::dec << "\n";
    std::cout << "  Run 2 per_job hash: 0x" << std::hex << hash2 << std::dec << "\n";
    if (hash1 == hash2 && run1.per_job_csv == run2.per_job_csv) {
        std::cout << "  [PASS] Determinism confirmed: 100k Hybrid outputs are identical byte-for-byte.\n\n";
    } else {
        std::cerr << "  [FAIL] Determinism error: 100k Hybrid outputs differ!\n\n";
        return 1;
    }

    // 6. Write CSV outputs
    std::cout << "[Step 5] Writing results/scaling.csv and copying to docs/scaling.csv...\n";
#ifdef _WIN32
    CreateDirectoryA("results", NULL);
    CreateDirectoryA("docs", NULL);
#endif

    std::ofstream csv_res("results/scaling.csv");
    std::ofstream csv_doc("docs/scaling.csv");

    std::string header = "workload,target_jobs,actual_jobs,policy,rep1_sec,rep2_sec,rep3_sec,median_sec,jobs_per_sec,simulated_ticks,busy_ticks,peak_ready_queue,peak_memory_mb,sanity_passed,status\n";
    csv_res << header;
    csv_doc << header;

    for (const auto& r : records) {
        std::ostringstream ss;
        ss << r.workload << ","
           << r.target_jobs << ","
           << r.actual_jobs << ","
           << r.policy << ","
           << std::fixed << std::setprecision(6)
           << r.rep_secs[0] << ","
           << r.rep_secs[1] << ","
           << r.rep_secs[2] << ","
           << r.median_sec << ","
           << std::setprecision(2) << r.jobs_per_sec << ","
           << r.simulated_ticks << ","
           << r.busy_ticks << ","
           << r.peak_ready_queue << ","
           << r.peak_memory_mb << ","
           << (r.sanity_passed ? "true" : "false") << ","
           << r.status << "\n";

        std::string line = ss.str();
        csv_res << line;
        csv_doc << line;
    }

    csv_res.close();
    csv_doc.close();

    std::cout << "  - Written: results/scaling.csv\n";
    std::cout << "  - Copied : docs/scaling.csv\n\n";

    std::cout << "[SUCCESS] Benchmark completed successfully.\n";
    return 0;
}
