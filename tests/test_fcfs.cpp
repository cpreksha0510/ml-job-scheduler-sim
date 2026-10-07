// ============================================================================
//  test_fcfs.cpp  –  Unit test for the FCFS scheduling policy
//
//  Build (standalone, no test framework):
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/policies/fcfs.cpp tests/test_fcfs.cpp
//        -o test_fcfs
//
//  Run:
//    test_fcfs
//
//  Test case: classic 5-process textbook example
//    P1(arr=0, burst=5)  P2(arr=1, burst=3)  P3(arr=2, burst=8)
//    P4(arr=3, burst=6)  P5(arr=4, burst=2)
//
//  Expected FCFS (non-preemptive) results:
//    Waiting times     : 0,  4,  6, 13, 18  (avg 8.2)
//    Turnaround times  : 5,  7, 14, 19, 20  (avg 13.0)
// ============================================================================
#include "simulator.hpp"
#include "policies/fcfs.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>   // memcmp (unused – kept for clarity)
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
    // Job::make(id, class, arrival, burst, priority, deadline, mem_req, label)
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

    const auto& done = sim.completed_jobs();

    // All 5 jobs must have completed.
    CHECK(done.size() == 5);
    if (done.size() != 5) {
        std::cerr << "  Skipping per-job checks (wrong completion count).\n";
        return;
    }

    // The Simulator sorts all_jobs_ by arrival internally (stable sort within
    // ties), so completed_ is ordered P1..P5 by arrival = id here.
    // Build a lookup by id to be robust against ordering.
    std::vector<const mlsched::Job*> by_id(6, nullptr); // index 1..5
    for (const auto& j : done) {
        by_id[j.id] = &j;
    }

    double total_wait = 0.0, total_ta = 0.0;

    for (int i = 0; i < 5; ++i) {
        uint32_t id = static_cast<uint32_t>(i + 1);
        const mlsched::Job* j = by_id[id];
        CHECK(j != nullptr);
        if (!j) continue;

        int64_t wt = j->waiting_time();
        int64_t ta = j->turnaround_time();

        CHECK(wt == exp_wait[i]);
        CHECK(ta == exp_turnaround[i]);

        total_wait += static_cast<double>(wt);
        total_ta   += static_cast<double>(ta);
    }

    // Verify averages (multiply by 10 to compare integers, avoiding float ==).
    // avg_wait * 10 == 82, avg_ta * 10 == 130
    int avg_wait_x10 = static_cast<int>(total_wait / 5.0 * 10.0 + 0.5);
    int avg_ta_x10   = static_cast<int>(total_ta   / 5.0 * 10.0 + 0.5);

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
    // The Simulator runs one idle tick (tick 0) then detects no pending/ready
    // jobs and breaks, returning current_tick_ == 1.
    CHECK(ticks == 1);
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

    CHECK(sim.completed_jobs().size() == 1);
    if (sim.completed_jobs().size() == 1) {
        const Job& j = sim.completed_jobs()[0];
        CHECK(j.waiting_time()    == 0);
        CHECK(j.turnaround_time() == 4);
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

    CHECK(sim.completed_jobs().size() == 2);
    if (sim.completed_jobs().size() == 2) {
        // Build lookup by id.
        std::vector<const Job*> by_id(3, nullptr);
        for (const auto& j : sim.completed_jobs()) by_id[j.id] = &j;

        // J1 runs first (id tie-break): wait=0, ta=3.
        CHECK(by_id[1]->waiting_time()    == 0);
        CHECK(by_id[1]->turnaround_time() == 3);
        // J2 runs second: wait=3-0=3, ta=5.
        CHECK(by_id[2]->waiting_time()    == 3);
        CHECK(by_id[2]->turnaround_time() == 5);
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
