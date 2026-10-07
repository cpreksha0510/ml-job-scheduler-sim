// ============================================================================
//  test_fcfs.cpp  –  Unit test for the FCFS scheduling policy
//
//  Build (standalone, no test framework):
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/policies/fcfs.cpp tests/test_fcfs.cpp
//        -o test_fcfs
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "policies/fcfs.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
//  Tiny assertion helper: prints a descriptive message on failure.
// ---------------------------------------------------------------------------
static int g_failures = 0;

static void check(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        std::cerr << "  ASSERTION FAILED: " << expr
                  << "  (" << file << ":" << line << ")\n";
        ++g_failures;
    }
}

#define CHECK(expr)  check((expr), #expr, __FILE__, __LINE__)

// ---------------------------------------------------------------------------
//  Build the 5-job workload (no CSV needed).
// ---------------------------------------------------------------------------
static std::vector<mlsched::Job> make_workload() {
    using mlsched::Job;
    using mlsched::JobClass;

    std::vector<Job> jobs;
    jobs.push_back(Job::make(1, JobClass::Training, 0, 5, 0, -1, 0, "P1"));
    jobs.push_back(Job::make(2, JobClass::Training, 1, 3, 0, -1, 0, "P2"));
    jobs.push_back(Job::make(3, JobClass::Training, 2, 8, 0, -1, 0, "P3"));
    jobs.push_back(Job::make(4, JobClass::Training, 3, 6, 0, -1, 0, "P4"));
    jobs.push_back(Job::make(5, JobClass::Training, 4, 2, 0, -1, 0, "P5"));
    return jobs;
}

// ---------------------------------------------------------------------------
//  Test: exact waiting and turnaround times for 5-job FCFS example.
// ---------------------------------------------------------------------------
static void test_fcfs_5job() {
    std::cout << "  Running test_fcfs_5job...\n";

    // Expected values (manually verified, see file header)
    const int64_t exp_wait[5]       = {0,  4,  6, 13, 18};
    const int64_t exp_turnaround[5] = {5,  7, 14, 19, 20};

    // Build and run the simulation.
    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(make_workload());
    sim.run(10000);

    auto metrics = mlsched::Metrics::calculate(sim, "FCFS");

    // All 5 jobs must have completed.
    CHECK(metrics.job_metrics().size() == 5);
    if (metrics.job_metrics().size() != 5) {
        std::cerr << "  Skipping per-job checks (wrong completion count).\n";
        return;
    }

    for (int i = 0; i < 5; ++i) {
        uint32_t id = static_cast<uint32_t>(i + 1);
        const auto& jm = metrics.job_metric(id);

        CHECK(jm.waiting_time == exp_wait[i]);
        CHECK(jm.turnaround_time == exp_turnaround[i]);
    }

    int avg_wait_x10 = static_cast<int>(std::round(metrics.overall().avg_waiting_time * 10.0));
    int avg_ta_x10   = static_cast<int>(std::round(metrics.overall().avg_turnaround_time * 10.0));

    CHECK(avg_wait_x10 == 82);   // avg = 8.2
    CHECK(avg_ta_x10   == 130);  // avg = 13.0
}

// ---------------------------------------------------------------------------
//  Test: empty workload must not crash and complete() list must be empty.
// ---------------------------------------------------------------------------
static void test_fcfs_empty() {
    std::cout << "  Running test_fcfs_empty...\n";
    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs({});
    int64_t ticks = sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(ticks == 1);
    CHECK(metrics.job_metrics().empty());
    CHECK(sim.completed_jobs().empty());
    CHECK(sim.incomplete_jobs().empty());
}

// ---------------------------------------------------------------------------
//  Test: single job — no waiting, turnaround == burst.
// ---------------------------------------------------------------------------
static void test_fcfs_single_job() {
    std::cout << "  Running test_fcfs_single_job...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 3, 4, 0, -1, 0, "Solo"));
    sim.run(1000);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 1);
    if (metrics.job_metrics().size() == 1) {
        const auto& jm = metrics.job_metric(1);
        CHECK(jm.waiting_time    == 0);
        CHECK(jm.turnaround_time == 4);
    }
}

// ---------------------------------------------------------------------------
//  Test: simultaneous arrivals — earlier id wins (determinism guarantee).
// ---------------------------------------------------------------------------
static void test_fcfs_simultaneous_arrival() {
    std::cout << "  Running test_fcfs_simultaneous_arrival...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    // Two jobs arrive at t=0; FCFS breaks ties by id (lower = first).
    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 3, 0, -1, 0, "A"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 2, 0, -1, 0, "B"));
    sim.run(1000);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        // J1 runs first (id tie-break): wait=0, ta=3.
        CHECK(metrics.job_metric(1).waiting_time    == 0);
        CHECK(metrics.job_metric(1).turnaround_time == 3);
        // J2 runs second: wait=3-0=3, ta=5.
        CHECK(metrics.job_metric(2).waiting_time    == 3);
        CHECK(metrics.job_metric(2).turnaround_time == 5);
    }
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "\n=== test_fcfs ===\n\n";

    test_fcfs_5job();
    test_fcfs_empty();
    test_fcfs_single_job();
    test_fcfs_simultaneous_arrival();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All FCFS tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
