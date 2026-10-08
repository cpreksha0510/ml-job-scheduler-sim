// ============================================================================
//  test_mlfq.cpp  –  Unit tests for the MLFQ scheduling policy
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/policies/mlfq.cpp tests/test_mlfq.cpp
//        -o test_mlfq
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "policies/mlfq.hpp"

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
//  Test 1: 5-job benchmark with quanta 2, 4, 8 and boost disabled
// ---------------------------------------------------------------------------
static void test_mlfq_5job() {
    std::cout << "  Running test_mlfq_5job...\n";

    const int64_t exp_finish[5]     = {13, 14, 24, 22, 10};
    const int64_t exp_wait[5]       = {8,  10, 14, 13, 4};
    const int64_t exp_turnaround[5] = {13, 13, 22, 19, 6};
    const int64_t exp_response[5]   = {0,  1,  2,  3,  4};

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 0));
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(make_5job_workload());
    sim.run(10000);

    auto metrics = mlsched::Metrics::calculate(sim, "MLFQ");
    CHECK(metrics.job_metrics().size() == 5);
    if (metrics.job_metrics().size() != 5) return;

    for (int i = 0; i < 5; ++i) {
        uint32_t id = static_cast<uint32_t>(i + 1);
        const auto& jm = metrics.job_metric(id);

        CHECK(jm.finish_time == exp_finish[i]);
        CHECK(jm.waiting_time == exp_wait[i]);
        CHECK(jm.turnaround_time == exp_turnaround[i]);
        CHECK(jm.response_time == exp_response[i]);
    }

    int avg_wait_x10 = static_cast<int>(std::round(metrics.overall().avg_waiting_time * 10.0));
    int avg_ta_x10   = static_cast<int>(std::round(metrics.overall().avg_turnaround_time * 10.0));
    int avg_rt_x10   = static_cast<int>(std::round(metrics.overall().avg_response_time * 10.0));

    CHECK(avg_wait_x10 == 98);   // avg 9.8
    CHECK(avg_ta_x10   == 146);  // avg 14.6
    CHECK(avg_rt_x10   == 20);   // avg 2.0
}

// ---------------------------------------------------------------------------
//  Test 2: Preemption case (P1 preempted at t=3 in Q1, stays in Q1, no demotion)
// ---------------------------------------------------------------------------
static void test_mlfq_preemption() {
    std::cout << "  Running test_mlfq_preemption...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 0));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 6, 0, -1, 0, "P1"));
    sim.add_job(Job::make(2, JobClass::Training, 3, 1, 0, -1, 0, "P2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        const auto& m_p1 = metrics.job_metric(1);
        const auto& m_p2 = metrics.job_metric(2);

        CHECK(m_p2.finish_time == 4);
        CHECK(m_p2.waiting_time == 0);

        CHECK(m_p1.finish_time == 7);
        CHECK(m_p1.waiting_time == 1);
    }

    // Verify execution segments: P1: [0, 2), [2, 3), [4, 7); P2: [3, 4)
    const auto& segs = sim.execution_segments();
    CHECK(segs.size() >= 3);
    if (segs.size() >= 3) {
        CHECK(segs[0].job_id == 1 && segs[0].start_tick == 0 && segs[0].end_tick == 3);
        CHECK(segs[1].job_id == 2 && segs[1].start_tick == 3 && segs[1].end_tick == 4);
        CHECK(segs[2].job_id == 1 && segs[2].start_tick == 4 && segs[2].end_tick == 7);
    }
}

// ---------------------------------------------------------------------------
//  Test 3: Priority boost case (quanta 2,4,8, S=6)
// ---------------------------------------------------------------------------
static void test_mlfq_boost() {
    std::cout << "  Running test_mlfq_boost...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 6));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 8, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 4, 3, 0, -1, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        const auto& m_j1 = metrics.job_metric(1);
        const auto& m_j2 = metrics.job_metric(2);

        CHECK(m_j1.finish_time == 11);
        CHECK(m_j1.waiting_time == 3);
        CHECK(m_j1.turnaround_time == 11);
        CHECK(m_j1.response_time == 0);

        CHECK(m_j2.finish_time == 9);
        CHECK(m_j2.waiting_time == 2);
        CHECK(m_j2.turnaround_time == 5);
        CHECK(m_j2.response_time == 0);
    }
}

// ---------------------------------------------------------------------------
//  Test 4: Simultaneous arrivals
// ---------------------------------------------------------------------------
static void test_mlfq_simultaneous_arrivals() {
    std::cout << "  Running test_mlfq_simultaneous_arrivals...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 0));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 3, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 3, 0, -1, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        CHECK(metrics.job_metric(1).finish_time == 5);
        CHECK(metrics.job_metric(1).waiting_time == 2);

        CHECK(metrics.job_metric(2).finish_time == 6);
        CHECK(metrics.job_metric(2).waiting_time == 3);
    }
}

// ---------------------------------------------------------------------------
//  Test 5: Identical jobs
// ---------------------------------------------------------------------------
static void test_mlfq_identical_jobs() {
    std::cout << "  Running test_mlfq_identical_jobs...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 0));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 2, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 2, 0, -1, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        CHECK(metrics.job_metric(1).finish_time == 2);
        CHECK(metrics.job_metric(1).waiting_time == 0);

        CHECK(metrics.job_metric(2).finish_time == 4);
        CHECK(metrics.job_metric(2).waiting_time == 2);
    }
}

// ---------------------------------------------------------------------------
//  Test 6: Zero-burst job (completes at arrival tick with waiting 0, no hang)
// ---------------------------------------------------------------------------
static void test_mlfq_zero_burst() {
    std::cout << "  Running test_mlfq_zero_burst...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Mlfq(2, 4, 8, 0));
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
    std::cout << "\n=== test_mlfq ===\n\n";

    test_mlfq_5job();
    test_mlfq_preemption();
    test_mlfq_boost();
    test_mlfq_simultaneous_arrivals();
    test_mlfq_identical_jobs();
    test_mlfq_zero_burst();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All MLFQ tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
