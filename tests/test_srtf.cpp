// ============================================================================
//  test_srtf.cpp  –  Unit tests for the SRTF (preemptive) scheduling policy
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/policies/srtf.cpp tests/test_srtf.cpp
//        -o test_srtf
// ============================================================================
#include "simulator.hpp"
#include "policies/srtf.hpp"

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
//  Test 1: 5-job standard workload
//  SRTF: waiting 5,0,14,7,0 avg 5.2 | turnaround 10,3,22,13,2 avg 10.0
//        response 0,0,14,7,0 avg 4.2
// ---------------------------------------------------------------------------
static void test_srtf_5job() {
    std::cout << "  Running test_srtf_5job...\n";

    const int64_t exp_wait[5]       = {5, 0, 14, 7, 0};
    const int64_t exp_turnaround[5] = {10, 3, 22, 13, 2};
    const int64_t exp_response[5]   = {0, 0, 14, 7, 0};

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Srtf());
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(make_5job_workload());
    sim.run(10000);

    const auto& done = sim.completed_jobs();
    CHECK(done.size() == 5);
    if (done.size() != 5) return;

    std::vector<const mlsched::Job*> by_id(6, nullptr);
    for (const auto& j : done) {
        by_id[j.id] = &j;
    }

    double total_wait = 0.0;
    double total_ta   = 0.0;
    double total_rt   = 0.0;

    for (int i = 0; i < 5; ++i) {
        uint32_t id = static_cast<uint32_t>(i + 1);
        const mlsched::Job* j = by_id[id];
        CHECK(j != nullptr);
        if (!j) continue;

        // For preemptive scheduling, true waiting time is turnaround - burst.
        int64_t wt = j->turnaround_time() - j->burst;
        int64_t ta = j->turnaround_time();
        int64_t rt = j->response_time();

        CHECK(wt == exp_wait[i]);
        CHECK(ta == exp_turnaround[i]);
        CHECK(rt == exp_response[i]);

        total_wait += static_cast<double>(wt);
        total_ta   += static_cast<double>(ta);
        total_rt   += static_cast<double>(rt);
    }

    int avg_wait_x10 = static_cast<int>(std::round((total_wait / 5.0) * 10.0));
    int avg_ta_x10   = static_cast<int>(std::round((total_ta / 5.0) * 10.0));
    int avg_rt_x10   = static_cast<int>(std::round((total_rt / 5.0) * 10.0));

    CHECK(avg_wait_x10 == 52);   // avg 5.2
    CHECK(avg_ta_x10   == 100);  // avg 10.0
    CHECK(avg_rt_x10   == 42);   // avg 4.2
}

// ---------------------------------------------------------------------------
//  Test 2: Simultaneous arrivals (tie-break by lowest job ID)
// ---------------------------------------------------------------------------
static void test_srtf_simultaneous_arrivals() {
    std::cout << "  Running test_srtf_simultaneous_arrivals...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Srtf());
    mlsched::Simulator sim(std::move(sched));
    // J1 burst 4, J2 burst 4 arrived at tick 0 -> tie broken by id: J1 runs first
    sim.add_job(Job::make(1, JobClass::Training, 0, 4, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 4, 0, -1, 0, "J2"));
    sim.run(100);

    const auto& done = sim.completed_jobs();
    CHECK(done.size() == 2);
    if (done.size() == 2) {
        std::vector<const Job*> by_id(3, nullptr);
        for (const auto& j : done) by_id[j.id] = &j;

        CHECK(by_id[1]->turnaround_time() - by_id[1]->burst == 0);
        CHECK(by_id[1]->turnaround_time() == 4);
        CHECK(by_id[1]->response_time() == 0);

        CHECK(by_id[2]->turnaround_time() - by_id[2]->burst == 4);
        CHECK(by_id[2]->turnaround_time() == 8);
        CHECK(by_id[2]->response_time() == 4);
    }
}

// ---------------------------------------------------------------------------
//  Test 3: Identical jobs
// ---------------------------------------------------------------------------
static void test_srtf_identical_jobs() {
    std::cout << "  Running test_srtf_identical_jobs...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Srtf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 3, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 3, 0, -1, 0, "J2"));
    sim.add_job(Job::make(3, JobClass::Training, 0, 3, 0, -1, 0, "J3"));
    sim.run(100);

    const auto& done = sim.completed_jobs();
    CHECK(done.size() == 3);
    if (done.size() == 3) {
        std::vector<const Job*> by_id(4, nullptr);
        for (const auto& j : done) by_id[j.id] = &j;

        CHECK(by_id[1]->turnaround_time() - by_id[1]->burst == 0);
        CHECK(by_id[1]->turnaround_time() == 3);
        CHECK(by_id[1]->response_time() == 0);

        CHECK(by_id[2]->turnaround_time() - by_id[2]->burst == 3);
        CHECK(by_id[2]->turnaround_time() == 6);
        CHECK(by_id[2]->response_time() == 3);

        CHECK(by_id[3]->turnaround_time() - by_id[3]->burst == 6);
        CHECK(by_id[3]->turnaround_time() == 9);
        CHECK(by_id[3]->response_time() == 6);
    }
}

// ---------------------------------------------------------------------------
//  Test 4: Zero-burst job (completes at arrival tick with waiting 0, no hang)
// ---------------------------------------------------------------------------
static void test_srtf_zero_burst() {
    std::cout << "  Running test_srtf_zero_burst...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Srtf());
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Training, 0, 0, 0, -1, 0, "Zero"));
    int64_t ticks = sim.run(50);

    CHECK(ticks <= 2);
    const auto& done = sim.completed_jobs();
    CHECK(done.size() == 1);
    if (done.size() == 1) {
        CHECK(done[0].waiting_time() == 0);
        CHECK(done[0].is_complete());
    }
}

int main() {
    std::cout << "\n=== test_srtf ===\n\n";

    test_srtf_5job();
    test_srtf_simultaneous_arrivals();
    test_srtf_identical_jobs();
    test_srtf_zero_burst();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All SRTF tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
