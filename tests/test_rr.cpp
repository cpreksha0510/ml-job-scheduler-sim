// ============================================================================
//  test_rr.cpp  –  Unit tests for the Round Robin scheduling policy
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/policies/rr.cpp tests/test_rr.cpp
//        -o test_rr
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "policies/rr.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

static int g_failures = 0;

static void check(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        std::cerr << "  ASSERTION FAILED: " << expr
                  << "  (" << file << ":" << line << ")\n";
        ++g_failures;
    }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static std::vector<mlsched::Job> make_5job_workload() {
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
//  Test 1: 5-job standard workload with quantum = 2
//  RR (quantum 2):
//    waiting 11,9,14,13,6 avg 10.6 | turnaround 16,12,22,19,8 avg 15.4
//    response 0,1,2,5,6   avg 2.8
// ---------------------------------------------------------------------------
static void test_rr_5job() {
    std::cout << "  Running test_rr_5job...\n";

    const int64_t exp_wait[5]       = {11, 9, 14, 13, 6};
    const int64_t exp_turnaround[5] = {16, 12, 22, 19, 8};
    const int64_t exp_response[5]   = {0, 1, 2, 5, 6};

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(make_5job_workload());
    sim.run(10000);

    auto metrics = mlsched::Metrics::calculate(sim, "Round Robin");
    CHECK(metrics.job_metrics().size() == 5);
    if (metrics.job_metrics().size() != 5) return;

    for (int i = 0; i < 5; ++i) {
        uint32_t id = static_cast<uint32_t>(i + 1);
        const auto& jm = metrics.job_metric(id);

        CHECK(jm.waiting_time == exp_wait[i]);
        CHECK(jm.turnaround_time == exp_turnaround[i]);
        CHECK(jm.response_time == exp_response[i]);
    }

    int avg_wait_x10 = static_cast<int>(std::round(metrics.overall().avg_waiting_time * 10.0));
    int avg_ta_x10   = static_cast<int>(std::round(metrics.overall().avg_turnaround_time * 10.0));
    int avg_rt_x10   = static_cast<int>(std::round(metrics.overall().avg_response_time * 10.0));

    CHECK(avg_wait_x10 == 106);  // avg 10.6
    CHECK(avg_ta_x10   == 154);  // avg 15.4
    CHECK(avg_rt_x10   == 28);   // avg 2.8
}

// ---------------------------------------------------------------------------
//  Test 2: Simultaneous arrivals (tie-break by lowest job ID)
// ---------------------------------------------------------------------------
static void test_rr_simultaneous_arrivals() {
    std::cout << "  Running test_rr_simultaneous_arrivals...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 3, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 3, 0, -1, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        CHECK(metrics.job_metric(1).waiting_time == 2);
        CHECK(metrics.job_metric(1).turnaround_time == 5);
        CHECK(metrics.job_metric(1).response_time == 0);

        CHECK(metrics.job_metric(2).waiting_time == 3);
        CHECK(metrics.job_metric(2).turnaround_time == 6);
        CHECK(metrics.job_metric(2).response_time == 2);
    }
}

// ---------------------------------------------------------------------------
//  Test 3: Identical jobs
// ---------------------------------------------------------------------------
static void test_rr_identical_jobs() {
    std::cout << "  Running test_rr_identical_jobs...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 2, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 2, 0, -1, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        CHECK(metrics.job_metric(1).waiting_time == 0);
        CHECK(metrics.job_metric(1).turnaround_time == 2);
        CHECK(metrics.job_metric(1).response_time == 0);

        CHECK(metrics.job_metric(2).waiting_time == 2);
        CHECK(metrics.job_metric(2).turnaround_time == 4);
        CHECK(metrics.job_metric(2).response_time == 2);
    }
}

// ---------------------------------------------------------------------------
//  Test 4: Zero-burst job (completes at arrival tick with waiting 0, no hang)
// ---------------------------------------------------------------------------
static void test_rr_zero_burst() {
    std::cout << "  Running test_rr_zero_burst...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 0, 0, -1, 0, "Zero"));
    int64_t ticks = sim.run(50);

    CHECK(ticks <= 2);
    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 1);
    if (metrics.job_metrics().size() == 1) {
        CHECK(metrics.job_metric(1).waiting_time == 0);
    }
}

int main() {
    std::cout << "\n=== test_rr ===\n\n";

    test_rr_5job();
    test_rr_simultaneous_arrivals();
    test_rr_identical_jobs();
    test_rr_zero_burst();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All RR tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
