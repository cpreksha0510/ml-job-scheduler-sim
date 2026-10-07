// ============================================================================
//  main.cpp  –  CLI driver for the ML Job Scheduler Simulator
//
//  Usage:
//    scheduler_sim <jobs.csv> [max_ticks]
//
//  Loads jobs from the CSV, runs a built-in FCFS stub (no policies yet),
//  and prints a per-tick timeline to stdout.
//
//  FCFS stub lives here temporarily until src/policies/fcfs.hpp is added
//  in the next task.  It must NOT be moved to a policy file until then.
// ============================================================================
#include "csv_loader.hpp"
#include "scheduler.hpp"
#include "simulator.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// ============================================================================
//  Temporary FCFS stub  (non-preemptive, O(n) scan – will be replaced)
// ============================================================================
namespace mlsched {

class FcfsStub final : public Scheduler {
public:
    const char* name() const noexcept override { return "FCFS (stub)"; }
    bool is_preemptive() const noexcept override { return false; }

    Job* pick_next(std::vector<Job*>& ready, int64_t /*time*/) override {
        if (ready.empty()) return nullptr;
        // FCFS: pick job with the smallest arrival time.
        return *std::min_element(ready.begin(), ready.end(),
            [](const Job* a, const Job* b){ return a->arrival < b->arrival; });
    }

    void on_tick(int64_t /*time*/) override {}
};

} // namespace mlsched

// ============================================================================
//  Helpers
// ============================================================================
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
    if (ev.is_preempted)              flags += "[PREEMPT]";
    if (job && job->is_complete())    flags += "[DONE]";
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

// ============================================================================
//  main
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <jobs.csv> [max_ticks]\n";
        return EXIT_FAILURE;
    }

    const std::string csv_path = argv[1];
    const int64_t max_ticks    = (argc >= 3) ? std::stoll(argv[2]) : 1'000'000;

    try {
        // Load jobs.
        auto jobs = mlsched::load_jobs_from_csv(csv_path);
        std::cout << "Loaded " << jobs.size() << " job(s) from " << csv_path << "\n";

        // Build simulator with FCFS stub.
        auto sched = std::make_unique<mlsched::FcfsStub>();
        const char* policy_name = sched->name();

        mlsched::Simulator sim(std::move(sched));
        sim.set_jobs(std::move(jobs));

        // Install live per-tick printer.
        print_header(policy_name);
        sim.set_tick_callback([](const mlsched::TickEvent& ev,
                                 const mlsched::Job* job)
        {
            print_event(ev, job);
        });

        // Run.
        int64_t ticks_run = sim.run(max_ticks);
        std::cout << std::string(60, '-') << "\n"
                  << "Simulation ended at tick " << ticks_run << "\n";

        // Print summary table.
        print_summary(sim);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
