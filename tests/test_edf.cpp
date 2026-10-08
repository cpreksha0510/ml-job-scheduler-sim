// ============================================================================
//  test_edf.cpp  –  Unit tests for the EDF (preemptive) scheduling policy
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/policies/edf.cpp tests/test_edf.cpp
//        -o test_edf
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "policies/edf.hpp"

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

// ---------------------------------------------------------------------------
//  Test 1: Feasible case
//  Jobs (arrival, burst, deadline): A(0,3,7), B(1,2,4), C(2,1,9)
//  Assert finish times A=5, B=3, C=6; 0 misses; waiting A=2, B=0, C=3.
// ---------------------------------------------------------------------------
static void test_edf_feasible() {
    std::cout << "  Running test_edf_feasible...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 3, 0, 7, 0, "A"));
    sim.add_job(Job::make(2, JobClass::Inference, 1, 2, 0, 4, 0, "B"));
    sim.add_job(Job::make(3, JobClass::Inference, 2, 1, 0, 9, 0, "C"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim, "EDF");
    CHECK(metrics.job_metrics().size() == 3);
    if (metrics.job_metrics().size() == 3) {
        const auto& mA = metrics.job_metric(1);
        const auto& mB = metrics.job_metric(2);
        const auto& mC = metrics.job_metric(3);

        CHECK(mA.finish_time == 5);
        CHECK(mA.waiting_time == 2);
        CHECK(!mA.missed_deadline);

        CHECK(mB.finish_time == 3);
        CHECK(mB.waiting_time == 0);
        CHECK(!mB.missed_deadline);

        CHECK(mC.finish_time == 6);
        CHECK(mC.waiting_time == 3);
        CHECK(!mC.missed_deadline);
    }

    CHECK(metrics.overall().deadline_miss_count == 0);
    CHECK(metrics.overall().deadline_miss_rate == 0.0);
}

// ---------------------------------------------------------------------------
//  Test 2: Overload case
//  All arriving at t=0: J1(0,4,3), J2(0,2,5), J3(0,2,6).
//  Finish times 4, 6, 8 and deadline-miss rate 1.0 (all three miss).
//  Note: Dropping J1 would have let J2 and J3 meet their deadlines.
// ---------------------------------------------------------------------------
static void test_edf_overload() {
    std::cout << "  Running test_edf_overload...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    // NOTE: In this overload scenario, pure EDF keeps executing J1 first due to its
    // earlier absolute deadline (3). J1 completes at t=4 (missing d=3).
    // Because J1 consumed 4 ticks, J2 finishes at t=6 (missing d=5), and J3 finishes
    // at t=8 (missing d=6), resulting in a domino failure where all 3 miss.
    // Dropping J1 would have let J2 (finish t=2 <= 5) and J3 (finish t=4 <= 6) meet their deadlines.
    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 4, 0, 3, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 2, 0, 5, 0, "J2"));
    sim.add_job(Job::make(3, JobClass::Inference, 0, 2, 0, 6, 0, "J3"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim, "EDF");
    CHECK(metrics.job_metrics().size() == 3);
    if (metrics.job_metrics().size() == 3) {
        const auto& m1 = metrics.job_metric(1);
        const auto& m2 = metrics.job_metric(2);
        const auto& m3 = metrics.job_metric(3);

        CHECK(m1.finish_time == 4);
        CHECK(m1.missed_deadline);

        CHECK(m2.finish_time == 6);
        CHECK(m2.missed_deadline);

        CHECK(m3.finish_time == 8);
        CHECK(m3.missed_deadline);
    }

    CHECK(metrics.overall().deadline_miss_count == 3);
    CHECK(metrics.overall().deadline_miss_rate == 1.0);
}

// ---------------------------------------------------------------------------
//  Test 3: Simultaneous arrivals (tie-break by lowest job ID)
// ---------------------------------------------------------------------------
static void test_edf_simultaneous_arrivals() {
    std::cout << "  Running test_edf_simultaneous_arrivals...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 2, 0, 5, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 2, 0, 5, 0, "J2"));
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
//  Test 4: Identical jobs
// ---------------------------------------------------------------------------
static void test_edf_identical_jobs() {
    std::cout << "  Running test_edf_identical_jobs...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 3, 0, 10, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 3, 0, 10, 0, "J2"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        CHECK(metrics.job_metric(1).finish_time == 3);
        CHECK(metrics.job_metric(1).waiting_time == 0);

        CHECK(metrics.job_metric(2).finish_time == 6);
        CHECK(metrics.job_metric(2).waiting_time == 3);
    }
}

// ---------------------------------------------------------------------------
//  Test 5: Zero-burst job (completes at arrival tick with waiting 0, no hang)
// ---------------------------------------------------------------------------
static void test_edf_zero_burst() {
    std::cout << "  Running test_edf_zero_burst...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 0, 0, 5, 0, "Zero"));
    int64_t ticks = sim.run(50);

    CHECK(ticks <= 2);
    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 1);
    if (metrics.job_metrics().size() == 1) {
        CHECK(metrics.job_metric(1).waiting_time == 0);
    }
}

// ---------------------------------------------------------------------------
//  Test 6: Job with no deadline (deadline == UNSET, treated as +infinity)
// ---------------------------------------------------------------------------
static void test_edf_no_deadline() {
    std::cout << "  Running test_edf_no_deadline...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    // J1 has no deadline (-1). J2 arrives at t=1 with deadline 5.
    // J2 has higher priority than J1 (+infinity), so J2 preempts J1.
    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Edf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 3, 0, mlsched::UNSET, 0, "NoDead"));
    sim.add_job(Job::make(2, JobClass::Inference, 1, 2, 0, 5, 0, "WithDead"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        const auto& m1 = metrics.job_metric(1);
        const auto& m2 = metrics.job_metric(2);

        // J2 runs at t=1..3, finishes at 3
        CHECK(m2.finish_time == 3);
        CHECK(m2.waiting_time == 0);
        CHECK(!m2.missed_deadline);

        // J1 runs at t=0..1, preempted, resumes t=3..5, finishes at 5
        CHECK(m1.finish_time == 5);
        CHECK(m1.waiting_time == 2);
        CHECK(!m1.has_deadline);
    }
}

int main() {
    std::cout << "\n=== test_edf ===\n\n";

    test_edf_feasible();
    test_edf_overload();
    test_edf_simultaneous_arrivals();
    test_edf_identical_jobs();
    test_edf_zero_burst();
    test_edf_no_deadline();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All EDF tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
