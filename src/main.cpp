// ============================================================================
//  main.cpp  –  CLI driver for the ML Job Scheduler Simulator
//
//  Usage (CLI mode):
//    sim.exe --policy <fcfs|sjf|srtf|rr|mlfq|edf|hybrid>
//            --workload <csv path> --out <folder>
//            [--quantum N] [--tier1-quantum N] [--tier2-quantum N]
//            [--aging N] [--reserve-k N]
//
//  Usage (Generation mode):
//    sim.exe --generate <config.cfg> [output.csv] [--seed N]
//
//  Usage (Legacy mode):
//    sim.exe <jobs.csv> [max_ticks]
// ============================================================================
#include "csv_loader.hpp"
#include "metrics.hpp"
#include "scheduler.hpp"
#include "simulator.hpp"
#include "workload.hpp"

#include "policies/fcfs.hpp"
#include "policies/sjf.hpp"
#include "policies/srtf.hpp"
#include "policies/rr.hpp"
#include "policies/mlfq.hpp"
#include "policies/edf.hpp"
#include "policies/hybrid.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// ============================================================================
//  Temporary FCFS stub  (non-preemptive, O(n) scan – preserved for legacy mode)
// ============================================================================
namespace mlsched {

class FcfsStub final : public Scheduler {
public:
    const char* name() const noexcept override { return "FCFS (stub)"; }
    bool is_preemptive() const noexcept override { return false; }

    Job* pick_next(std::vector<Job*>& ready, int64_t /*time*/) override {
        if (ready.empty()) return nullptr;
        return *std::min_element(ready.begin(), ready.end(),
            [](const Job* a, const Job* b){ return a->arrival < b->arrival; });
    }

    void on_tick(int64_t /*time*/) override {}
};

} // namespace mlsched

// ============================================================================
//  Helpers
// ============================================================================
static std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static void ensure_dir_exists(const std::string& path) {
    if (path.empty()) return;
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

static bool parse_int64(const std::string& str, int64_t& out_val) {
    if (str.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long long val = std::strtoll(str.c_str(), &end, 10);
    if (errno != 0 || end == str.c_str() || *end != '\0') return false;
    out_val = static_cast<int64_t>(val);
    return true;
}

static void print_header(const char* policy_name) {
    std::cout << "\n=== ML Job Scheduler Simulator ===\n"
              << "Policy : " << policy_name << "\n"
              << std::string(60, '-') << "\n"
              << std::left
              << std::setw(7)  << "Tick"
              << std::setw(10) << "JobID"
              << std::setw(10) << "Label"
              << std::setw(10) << "Class"
              << std::setw(12) << "Remaining"
              << std::setw(12) << "Flags"
              << "\n"
              << std::string(60, '-') << "\n";
}

static void print_event(const mlsched::TickEvent& ev, const mlsched::Job* job) {
    using namespace mlsched;
    std::cout << std::left
              << std::setw(7)  << ev.tick;

    if (ev.is_idle) {
        std::cout << std::setw(10) << "--"
                  << std::setw(10) << "IDLE"
                  << std::setw(10) << "--"
                  << std::setw(12) << "--"
                  << std::setw(12) << ""
                  << "\n";
        return;
    }

    std::string flags;
    if (ev.is_preempted)               flags += "[PREEMPT]";
    if (job && job->is_complete())     flags += "[DONE]";
    if (job && job->missed_deadline()) flags += "[MISS]";

    std::cout << std::setw(10) << ev.job_id
              << std::setw(10) << (job ? job->label : "?")
              << std::setw(10) << (job ? to_string(job->job_class) : "?")
              << std::setw(12) << ev.remaining_after
              << std::setw(12) << flags
              << "\n";
}

static void print_summary(const mlsched::Simulator& sim) {
    using namespace mlsched;
    const auto& done = sim.completed_jobs();

    std::cout << "\n" << std::string(60, '=') << "\n"
              << "Summary  (" << done.size() << " jobs completed)\n"
              << std::string(60, '-') << "\n"
              << std::left
              << std::setw(6)  << "ID"
              << std::setw(10) << "Label"
              << std::setw(10) << "Class"
              << std::setw(10) << "Arrival"
              << std::setw(8)  << "Burst"
              << std::setw(10) << "Finish"
              << std::setw(10) << "Wait"
              << std::setw(12) << "Turnaround"
              << std::setw(10) << "Response"
              << std::setw(8)  << "Miss?"
              << "\n"
              << std::string(90, '-') << "\n";

    double total_wait = 0, total_ta = 0, total_rt = 0;
    int    misses = 0;

    for (const auto& j : done) {
        bool miss = j.missed_deadline();
        misses += miss ? 1 : 0;
        total_wait += static_cast<double>(j.waiting_time());
        total_ta   += static_cast<double>(j.turnaround_time());
        total_rt   += static_cast<double>(j.response_time());

        std::cout << std::setw(6)  << j.id
                  << std::setw(10) << j.label
                  << std::setw(10) << to_string(j.job_class)
                  << std::setw(10) << j.arrival
                  << std::setw(8)  << j.burst
                  << std::setw(10) << j.finish_time
                  << std::setw(10) << j.waiting_time()
                  << std::setw(12) << j.turnaround_time()
                  << std::setw(10) << j.response_time()
                  << std::setw(8)  << (miss ? "YES" : "no")
                  << "\n";
    }

    if (!done.empty()) {
        double n = static_cast<double>(done.size());
        std::cout << std::string(90, '-') << "\n"
                  << "Avg  Wait: " << total_wait / n
                  << "   Turnaround: " << total_ta / n
                  << "   Response: "  << total_rt / n
                  << "\n"
                  << "Deadline misses: " << misses << " / " << done.size()
                  << "\n";
    }

    if (!sim.incomplete_jobs().empty()) {
        std::cout << "\n[WARNING] " << sim.incomplete_jobs().size()
                  << " job(s) did not complete within max_ticks.\n";
    }
}

static void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  CLI Mode:\n"
              << "    " << prog << " --policy <fcfs|sjf|srtf|rr|mlfq|edf|hybrid>\n"
              << "              --workload <csv path> --out <folder>\n"
              << "              [--quantum N] [--tier1-quantum N] [--tier2-quantum N]\n"
              << "              [--aging N] [--reserve-k N]\n"
              << "  Workload Generation Mode:\n"
              << "    " << prog << " --generate <config.cfg> [output.csv] [--seed N]\n"
              << "  Legacy Mode:\n"
              << "    " << prog << " <jobs.csv> [max_ticks]\n";
}

// ============================================================================
//  main
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 1. Generation mode
    if (std::string(argv[1]) == "--generate" || std::string(argv[1]) == "-g") {
        if (argc < 3) {
            std::cerr << "Error: --generate requires at least a config file path.\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        try {
            const std::string cfg_path = argv[2];
            auto cfg = mlsched::WorkloadConfig::load_from_file(cfg_path);

            std::string out_csv;
            for (int i = 3; i < argc; ++i) {
                std::string arg = argv[i];
                if ((arg == "--seed" || arg == "-s") && i + 1 < argc) {
                    int64_t s = 0;
                    if (!parse_int64(argv[++i], s) || s < 0) {
                        std::cerr << "Error: Invalid seed value: " << argv[i] << "\n";
                        return EXIT_FAILURE;
                    }
                    cfg.seed = static_cast<uint32_t>(s);
                } else if (arg.rfind("--", 0) != 0 && out_csv.empty()) {
                    out_csv = arg;
                }
            }

            mlsched::WorkloadGenerator gen(cfg);
            auto jobs = gen.generate();
            auto summary = mlsched::WorkloadGenerator::summarize(jobs, cfg.horizon);

            std::cout << "\n=== Workload Generation Summary ===\n"
                      << "Config file  : " << cfg_path << "\n"
                      << "Seed         : " << cfg.seed << "\n"
                      << "Horizon      : " << summary.horizon << " ticks\n"
                      << "Total jobs   : " << summary.total_jobs << "\n"
                      << "  - Inference    : " << summary.infer_jobs
                      << " jobs (demand: " << summary.infer_demand << " ticks)\n"
                      << "  - Training     : " << summary.train_jobs
                      << " jobs (demand: " << summary.train_demand << " ticks)\n"
                      << "  - Preprocessing: " << summary.preproc_jobs
                      << " jobs (demand: " << summary.preproc_demand << " ticks)\n"
                      << "Total demand : " << summary.total_demand << " ticks\n"
                      << "Capacity     : " << summary.horizon << " ticks\n"
                      << "Demand ratio : " << std::fixed << std::setprecision(1)
                      << (summary.demand_ratio * 100.0) << "% ("
                      << (summary.demand_ratio > 1.0 ? "OVERLOADED" : (summary.demand_ratio < 0.6 ? "LIGHT" : "MEDIUM"))
                      << ")\n";

            if (!out_csv.empty()) {
                if (mlsched::save_jobs_to_csv(out_csv, jobs)) {
                    std::cout << "Saved " << jobs.size() << " jobs to " << out_csv << "\n";
                } else {
                    std::cerr << "Failed to write CSV to " << out_csv << "\n";
                    return EXIT_FAILURE;
                }
            }
            return EXIT_SUCCESS;
        } catch (const std::exception& e) {
            std::cerr << "Generation error: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
    }

    // 2. Determine if invoked with flags or legacy positional syntax
    bool has_flags = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]).rfind("-", 0) == 0) {
            has_flags = true;
            break;
        }
    }

    // Legacy mode: sim.exe <jobs.csv> [max_ticks]
    if (!has_flags) {
        const std::string csv_path = argv[1];
        int64_t max_ticks = 1'000'000;
        if (argc >= 3) {
            if (!parse_int64(argv[2], max_ticks)) {
                std::cerr << "Error: Invalid max_ticks: " << argv[2] << "\n";
                return EXIT_FAILURE;
            }
        }

        try {
            auto jobs = mlsched::load_jobs_from_csv(csv_path);
            std::cout << "Loaded " << jobs.size() << " job(s) from " << csv_path << "\n";

            auto sched = std::make_unique<mlsched::FcfsStub>();
            const char* policy_name = sched->name();

            mlsched::Simulator sim(std::move(sched));
            sim.set_jobs(std::move(jobs));

            print_header(policy_name);
            sim.set_tick_callback([](const mlsched::TickEvent& ev, const mlsched::Job* job) {
                print_event(ev, job);
            });

            int64_t ticks_run = sim.run(max_ticks);
            std::cout << std::string(60, '-') << "\n"
                      << "Simulation ended at tick " << ticks_run << "\n";
            print_summary(sim);
            return EXIT_SUCCESS;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
    }

    // 3. New CLI mode
    std::string policy_arg;
    std::string workload_arg;
    std::string out_arg;
    int64_t quantum_val        = 2;  // default for RR
    bool    quantum_set        = false;
    int64_t tier1_quantum_val  = 8;  // default for Hybrid
    bool    tier1_quantum_set  = false;
    int64_t tier2_quantum_val  = 32; // default for Hybrid
    bool    tier2_quantum_set  = false;
    int64_t aging_val          = 100;// default for Hybrid
    bool    aging_set          = false;
    int64_t reserve_k_val      = 10; // default for Hybrid
    bool    reserve_k_set      = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--policy" || arg == "-p") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag " << arg << " requires a policy argument.\n";
                return EXIT_FAILURE;
            }
            policy_arg = argv[++i];
        } else if (arg == "--workload" || arg == "-w") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag " << arg << " requires a workload CSV path.\n";
                return EXIT_FAILURE;
            }
            workload_arg = argv[++i];
        } else if (arg == "--out" || arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag " << arg << " requires an output directory path.\n";
                return EXIT_FAILURE;
            }
            out_arg = argv[++i];
        } else if (arg == "--quantum") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag --quantum requires an integer value.\n";
                return EXIT_FAILURE;
            }
            if (!parse_int64(argv[++i], quantum_val) || quantum_val <= 0) {
                std::cerr << "Error: --quantum must be a positive integer, got: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            quantum_set = true;
        } else if (arg == "--tier1-quantum") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag --tier1-quantum requires an integer value.\n";
                return EXIT_FAILURE;
            }
            if (!parse_int64(argv[++i], tier1_quantum_val) || tier1_quantum_val <= 0) {
                std::cerr << "Error: --tier1-quantum must be a positive integer, got: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            tier1_quantum_set = true;
        } else if (arg == "--tier2-quantum") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag --tier2-quantum requires an integer value.\n";
                return EXIT_FAILURE;
            }
            if (!parse_int64(argv[++i], tier2_quantum_val) || tier2_quantum_val <= 0) {
                std::cerr << "Error: --tier2-quantum must be a positive integer, got: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            tier2_quantum_set = true;
        } else if (arg == "--aging") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag --aging requires an integer value.\n";
                return EXIT_FAILURE;
            }
            if (!parse_int64(argv[++i], aging_val) || aging_val < 0) {
                std::cerr << "Error: --aging must be a non-negative integer, got: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            aging_set = true;
        } else if (arg == "--reserve-k") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Flag --reserve-k requires an integer value.\n";
                return EXIT_FAILURE;
            }
            if (!parse_int64(argv[++i], reserve_k_val) || reserve_k_val < 0) {
                std::cerr << "Error: --reserve-k must be a non-negative integer, got: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            reserve_k_set = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Error: Unrecognized argument: " << arg << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (policy_arg.empty()) {
        std::cerr << "Error: Missing required argument: --policy <fcfs|sjf|srtf|rr|mlfq|edf|hybrid>\n";
        return EXIT_FAILURE;
    }
    if (workload_arg.empty()) {
        std::cerr << "Error: Missing required argument: --workload <csv path>\n";
        return EXIT_FAILURE;
    }
    if (out_arg.empty()) {
        std::cerr << "Error: Missing required argument: --out <folder>\n";
        return EXIT_FAILURE;
    }

    // Instantiate policy
    std::string pol = to_lower_str(policy_arg);
    std::unique_ptr<mlsched::Scheduler> sched;

    if (pol == "fcfs") {
        sched = std::make_unique<mlsched::Fcfs>();
    } else if (pol == "sjf") {
        sched = std::make_unique<mlsched::Sjf>();
    } else if (pol == "srtf") {
        sched = std::make_unique<mlsched::Srtf>();
    } else if (pol == "rr" || pol == "roundrobin" || pol == "round_robin") {
        int64_t q = quantum_set ? quantum_val : 2;
        sched = std::make_unique<mlsched::RoundRobin>(q);
    } else if (pol == "mlfq") {
        sched = std::make_unique<mlsched::Mlfq>();
    } else if (pol == "edf") {
        sched = std::make_unique<mlsched::Edf>();
    } else if (pol == "hybrid") {
        mlsched::HybridConfig cfg;
        if (tier1_quantum_set) cfg.tier1_quantum = tier1_quantum_val;
        if (tier2_quantum_set) cfg.tier2_quantum = tier2_quantum_val;
        if (aging_set)         cfg.aging_threshold = aging_val;
        if (reserve_k_set)     cfg.reserve_period = reserve_k_val;
        sched = std::make_unique<mlsched::HybridScheduler>(cfg);
    } else {
        std::cerr << "Error: Unknown policy '" << policy_arg
                  << "'. Valid policies: fcfs, sjf, srtf, rr, mlfq, edf, hybrid\n";
        return EXIT_FAILURE;
    }

    // Load workload
    std::vector<mlsched::Job> jobs;
    try {
        jobs = mlsched::load_jobs_from_csv(workload_arg);
    } catch (const std::exception& e) {
        std::cerr << "Error loading workload CSV: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // Run simulation
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(std::move(jobs));
    sim.run(2'000'000);

    // Calculate metrics and export CSVs
    auto metrics = mlsched::Metrics::calculate(sim, policy_arg);

    ensure_dir_exists(out_arg);
    std::string prefix = out_arg;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') {
        prefix += "/";
    }
    // Remove previous results.csv in output directory so export_all creates a clean file
    std::remove((prefix + "results.csv").c_str());

    if (!metrics.export_all(out_arg)) {
        std::cerr << "Error: Failed to export simulation CSVs to " << out_arg << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Simulation successful for policy '" << policy_arg
              << "'. Output written to " << out_arg << "\n";
    return EXIT_SUCCESS;
}
