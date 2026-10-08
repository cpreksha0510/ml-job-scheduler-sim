// ============================================================================
//  experiment.cpp  –  Benchmark Experiment Runner
//
//  Evaluates 8 scheduling policies across 3 load levels and 10 random seeds.
//  Writes results/runs_raw.csv, results/summary.csv, and timeline CSVs for Gantt.
//
//  C++14, STL only.
// ============================================================================
#include "csv_loader.hpp"
#include "metrics.hpp"
#include "simulator.hpp"
#include "workload.hpp"

#include "policies/fcfs.hpp"
#include "policies/sjf.hpp"
#include "policies/srtf.hpp"
#include "policies/rr.hpp"
#include "policies/mlfq.hpp"
#include "policies/edf.hpp"
#include "policies/hybrid.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {

void ensure_dir(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
#ifdef _WIN32
        _mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0755);
#endif
    }
}

struct PolicyFactory {
    std::string name;
    std::function<std::unique_ptr<mlsched::Scheduler>()> create;
};

struct Stats {
    double mean = 0.0;
    double std  = 0.0;
    size_t n    = 0;
};

Stats compute_stats(const std::vector<double>& vals) {
    Stats s;
    s.n = vals.size();
    if (vals.empty()) return s;

    double sum = 0.0;
    for (double v : vals) sum += v;
    s.mean = sum / static_cast<double>(s.n);

    if (s.n > 1) {
        double var = 0.0;
        for (double v : vals) {
            double diff = v - s.mean;
            var += diff * diff;
        }
        s.std = std::sqrt(var / static_cast<double>(s.n - 1)); // Sample std
    } else {
        s.std = 0.0;
    }
    return s;
}

struct RawRow {
    std::string policy;
    std::string load;
    int seed;
    size_t total_jobs;
    int64_t makespan;
    double throughput;
    double avg_wait;
    double avg_ta;
    double avg_resp;
    size_t deadline_count;
    size_t deadline_miss_count;
    double deadline_miss_rate;
    int64_t max_wait;
    size_t starved_count;

    // Per-class metrics
    double infer_avg_wait;
    double infer_avg_ta;
    double infer_avg_resp;
    double infer_miss_rate;
    int64_t infer_max_wait;
    size_t infer_starved;

    double train_avg_wait;
    double train_avg_ta;
    double train_avg_resp;
    double train_miss_rate;
    int64_t train_max_wait;
    size_t train_starved;

    double preproc_avg_wait;
    double preproc_avg_ta;
    double preproc_avg_resp;
    double preproc_miss_rate;
    int64_t preproc_max_wait;
    size_t preproc_starved;
};

} // namespace

int main(int argc, char* argv[]) {
    bool quiet = (argc > 1 && std::string(argv[1]) == "--quiet");

    if (!quiet) {
        std::cout << "============================================================\n"
                  << " ML Job Scheduler Simulator -- Benchmark Experiments\n"
                  << "============================================================\n\n";
    }

    ensure_dir("results/summary.csv");

    // 1. Define Policies
    // RR-small = 2 (median inference burst across light & medium workloads)
    // RR-large = 8 (4x RR-small, matches hybrid tier1 quantum)
    std::vector<PolicyFactory> policies = {
        {"FCFS",     [](){ return std::make_unique<mlsched::Fcfs>(); }},
        {"SJF",      [](){ return std::make_unique<mlsched::Sjf>(); }},
        {"SRTF",     [](){ return std::make_unique<mlsched::Srtf>(); }},
        {"RR-small", [](){ return std::make_unique<mlsched::RoundRobin>(2); }},
        {"RR-large", [](){ return std::make_unique<mlsched::RoundRobin>(8); }},
        {"MLFQ",     [](){ return std::make_unique<mlsched::Mlfq>(); }},
        {"EDF",      [](){ return std::make_unique<mlsched::Edf>(); }},
        {"Hybrid",   [](){ return std::make_unique<mlsched::HybridScheduler>(); }}
    };

    // 2. Define Workload Load Levels
    struct LoadLevel {
        std::string name;
        std::string cfg_path;
    };
    std::vector<LoadLevel> loads = {
        {"light",      "data/workload_light.cfg"},
        {"medium",     "data/workload_medium.cfg"},
        {"overloaded", "data/workload_overloaded.cfg"}
    };

    const std::vector<int> seeds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    std::vector<RawRow> raw_rows;

    // Map: key = (policy, load, class_name, metric_name) -> vector of 10 seed values
    using MetricKey = std::tuple<std::string, std::string, std::string, std::string>;
    std::map<MetricKey, std::vector<double>> agg_map;

    for (const auto& load : loads) {
        if (!quiet) {
            std::cout << "--> Running load level: " << load.name << "\n";
        }

        auto base_cfg = mlsched::WorkloadConfig::load_from_file(load.cfg_path);

        for (int seed : seeds) {
            // Generate workload ONCE per (load, seed)
            auto cfg = base_cfg;
            cfg.seed = static_cast<uint32_t>(seed);
            mlsched::WorkloadGenerator gen(cfg);
            auto workload_jobs = gen.generate();

            int64_t total_burst = 0;
            for (const auto& j : workload_jobs) {
                total_burst += j.burst;
            }

            for (const auto& p : policies) {
                auto sched = p.create();
                mlsched::Simulator sim(std::move(sched));
                sim.set_jobs(workload_jobs); // Pass copy of jobs
                sim.run(2'000'000);

                // --- Sanity Check 1: Every policy completes the same set of jobs ---
                if (!sim.incomplete_jobs().empty() || sim.completed_jobs().size() != workload_jobs.size()) {
                    std::cerr << "[FATAL SANITY CHECK ERROR] Policy " << p.name
                              << " at load " << load.name << " seed " << seed
                              << " completed " << sim.completed_jobs().size()
                              << "/" << workload_jobs.size() << " jobs!\n";
                    std::abort();
                }

                // --- Sanity Check 2: Total busy ticks equals sum of bursts ---
                int64_t busy_ticks = 0;
                for (const auto& ev : sim.timeline()) {
                    if (!ev.is_idle) ++busy_ticks;
                }
                if (busy_ticks != total_burst) {
                    std::cerr << "[FATAL SANITY CHECK ERROR] Policy " << p.name
                              << " at load " << load.name << " seed " << seed
                              << ": busy ticks (" << busy_ticks << ") != total burst ("
                              << total_burst << ")!\n";
                    std::abort();
                }

                auto m = mlsched::Metrics::calculate(sim, p.name, 100);
                const auto& overall = m.overall();
                const auto& inf     = m.class_metrics(mlsched::JobClass::Inference);
                const auto& trn     = m.class_metrics(mlsched::JobClass::Training);
                const auto& prp     = m.class_metrics(mlsched::JobClass::Preprocessing);

                RawRow rr;
                rr.policy              = p.name;
                rr.load                = load.name;
                rr.seed                = seed;
                rr.total_jobs          = overall.total_jobs;
                rr.makespan            = overall.makespan;
                rr.throughput          = overall.throughput;
                rr.avg_wait            = overall.avg_waiting_time;
                rr.avg_ta              = overall.avg_turnaround_time;
                rr.avg_resp            = overall.avg_response_time;
                rr.deadline_count      = overall.deadline_count;
                rr.deadline_miss_count = overall.deadline_miss_count;
                rr.deadline_miss_rate  = overall.deadline_miss_rate;
                rr.max_wait            = overall.max_waiting_time;
                rr.starved_count       = overall.starved_count;

                rr.infer_avg_wait      = inf.avg_waiting_time;
                rr.infer_avg_ta        = inf.avg_turnaround_time;
                rr.infer_avg_resp      = inf.avg_response_time;
                rr.infer_miss_rate     = inf.deadline_miss_rate;
                rr.infer_max_wait      = inf.max_waiting_time;
                rr.infer_starved       = inf.starved_count;

                rr.train_avg_wait      = trn.avg_waiting_time;
                rr.train_avg_ta        = trn.avg_turnaround_time;
                rr.train_avg_resp      = trn.avg_response_time;
                rr.train_miss_rate     = trn.deadline_miss_rate;
                rr.train_max_wait      = trn.max_waiting_time;
                rr.train_starved       = trn.starved_count;

                rr.preproc_avg_wait    = prp.avg_waiting_time;
                rr.preproc_avg_ta      = prp.avg_turnaround_time;
                rr.preproc_avg_resp    = prp.avg_response_time;
                rr.preproc_miss_rate   = prp.deadline_miss_rate;
                rr.preproc_max_wait    = prp.max_waiting_time;
                rr.preproc_starved     = prp.starved_count;

                raw_rows.push_back(rr);

                // Populate aggregation maps
                auto record_metric = [&](const std::string& cls, const std::string& metric, double val) {
                    agg_map[std::make_tuple(p.name, load.name, cls, metric)].push_back(val);
                };

                // Overall
                record_metric("overall", "avg_waiting_time", overall.avg_waiting_time);
                record_metric("overall", "avg_turnaround_time", overall.avg_turnaround_time);
                record_metric("overall", "avg_response_time", overall.avg_response_time);
                record_metric("overall", "deadline_miss_rate", overall.deadline_miss_rate);
                record_metric("overall", "throughput", overall.throughput);
                record_metric("overall", "max_waiting_time", static_cast<double>(overall.max_waiting_time));
                record_metric("overall", "starved_count", static_cast<double>(overall.starved_count));

                // Inference
                record_metric("inference", "avg_waiting_time", inf.avg_waiting_time);
                record_metric("inference", "avg_turnaround_time", inf.avg_turnaround_time);
                record_metric("inference", "avg_response_time", inf.avg_response_time);
                record_metric("inference", "deadline_miss_rate", inf.deadline_miss_rate);
                record_metric("inference", "throughput", inf.throughput);
                record_metric("inference", "max_waiting_time", static_cast<double>(inf.max_waiting_time));
                record_metric("inference", "starved_count", static_cast<double>(inf.starved_count));

                // Training
                record_metric("training", "avg_waiting_time", trn.avg_waiting_time);
                record_metric("training", "avg_turnaround_time", trn.avg_turnaround_time);
                record_metric("training", "avg_response_time", trn.avg_response_time);
                record_metric("training", "deadline_miss_rate", trn.deadline_miss_rate);
                record_metric("training", "throughput", trn.throughput);
                record_metric("training", "max_waiting_time", static_cast<double>(trn.max_waiting_time));
                record_metric("training", "starved_count", static_cast<double>(trn.starved_count));

                // Preprocessing
                record_metric("preprocessing", "avg_waiting_time", prp.avg_waiting_time);
                record_metric("preprocessing", "avg_turnaround_time", prp.avg_turnaround_time);
                record_metric("preprocessing", "avg_response_time", prp.avg_response_time);
                record_metric("preprocessing", "deadline_miss_rate", prp.deadline_miss_rate);
                record_metric("preprocessing", "throughput", prp.throughput);
                record_metric("preprocessing", "max_waiting_time", static_cast<double>(prp.max_waiting_time));
                record_metric("preprocessing", "starved_count", static_cast<double>(prp.starved_count));
            }
        }
    }

    // 3. Write results/runs_raw.csv
    {
        std::ofstream out("results/runs_raw.csv");
        if (!out.is_open()) {
            std::cerr << "Failed to open results/runs_raw.csv for writing.\n";
            return EXIT_FAILURE;
        }
        out << "policy,load,seed,total_jobs,makespan,throughput,avg_wait,avg_ta,avg_resp,"
            << "deadline_count,deadline_miss_count,deadline_miss_rate,max_wait,starved_count,"
            << "infer_avg_wait,infer_avg_ta,infer_avg_resp,infer_miss_rate,infer_max_wait,infer_starved,"
            << "train_avg_wait,train_avg_ta,train_avg_resp,train_miss_rate,train_max_wait,train_starved,"
            << "preproc_avg_wait,preproc_avg_ta,preproc_avg_resp,preproc_miss_rate,preproc_max_wait,preproc_starved\n";

        for (const auto& r : raw_rows) {
            out << r.policy << "," << r.load << "," << r.seed << ","
                << r.total_jobs << "," << r.makespan << "," << std::setprecision(6) << r.throughput << ","
                << r.avg_wait << "," << r.avg_ta << "," << r.avg_resp << ","
                << r.deadline_count << "," << r.deadline_miss_count << "," << r.deadline_miss_rate << ","
                << r.max_wait << "," << r.starved_count << ","
                << r.infer_avg_wait << "," << r.infer_avg_ta << "," << r.infer_avg_resp << ","
                << r.infer_miss_rate << "," << r.infer_max_wait << "," << r.infer_starved << ","
                << r.train_avg_wait << "," << r.train_avg_ta << "," << r.train_avg_resp << ","
                << r.train_miss_rate << "," << r.train_max_wait << "," << r.train_starved << ","
                << r.preproc_avg_wait << "," << r.preproc_avg_ta << "," << r.preproc_avg_resp << ","
                << r.preproc_miss_rate << "," << r.preproc_max_wait << "," << r.preproc_starved << "\n";
        }
    }

    // 4. Write results/summary.csv
    {
        std::ofstream out("results/summary.csv");
        if (!out.is_open()) {
            std::cerr << "Failed to open results/summary.csv for writing.\n";
            return EXIT_FAILURE;
        }
        out << "policy,load,class,metric,mean,std,n\n";

        for (const auto& kv : agg_map) {
            std::string policy, load, cls, metric;
            std::tie(policy, load, cls, metric) = kv.first;
            Stats s = compute_stats(kv.second);

            out << policy << "," << load << "," << cls << "," << metric << ","
                << std::fixed << std::setprecision(6) << s.mean << ","
                << std::fixed << std::setprecision(6) << s.std << ","
                << s.n << "\n";
        }
    }

    // 5. Generate small light workload (about 30 jobs) for Gantt chart export
    {
        auto g_cfg = mlsched::WorkloadConfig::load_from_file("data/workload_light.cfg");
        g_cfg.seed = 42;
        g_cfg.horizon = 350; // Produces ~30 jobs
        mlsched::WorkloadGenerator gen(g_cfg);
        auto g_jobs = gen.generate();

        // Run Hybrid
        {
            auto sched = std::make_unique<mlsched::HybridScheduler>();
            mlsched::Simulator sim(std::move(sched));
            sim.set_jobs(g_jobs);
            sim.run(10000);
            auto m = mlsched::Metrics::calculate(sim, "Hybrid");
            m.export_timeline_csv("results/timeline_hybrid.csv");
        }

        // Run RR-small
        {
            auto sched = std::make_unique<mlsched::RoundRobin>(2);
            mlsched::Simulator sim(std::move(sched));
            sim.set_jobs(g_jobs);
            sim.run(10000);
            auto m = mlsched::Metrics::calculate(sim, "Round Robin");
            m.export_timeline_csv("results/timeline_rr.csv");
        }
    }

    if (!quiet) {
        std::cout << "\n[SUCCESS] Experiments completed successfully.\n"
                  << "  - Written: results/runs_raw.csv\n"
                  << "  - Written: results/summary.csv\n"
                  << "  - Written: results/timeline_hybrid.csv\n"
                  << "  - Written: results/timeline_rr.csv\n\n";
    }

    return EXIT_SUCCESS;
}
