// ============================================================================
//  test_metrics.cpp  –  Unit tests for Metrics module & CSV exports
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/policies/fcfs.cpp
//        src/policies/rr.cpp tests/test_metrics.cpp -o test_metrics
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "policies/fcfs.hpp"
#include "policies/rr.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
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
//  Test 1: Deadline-Miss Rate
//  Jobs with deadline: 4 (J1 met, J2 missed, J3 met, J4 missed)
//  Job without deadline: J5 (excluded from rate)
//  Expected miss rate = 2 / 4 = 0.50 (50.0%)
// ---------------------------------------------------------------------------
static void test_deadline_miss_rate() {
    std::cout << "  Running test_deadline_miss_rate...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));

    // Under FCFS:
    // J1: arr 0, burst 2, ddl 3 -> finish 2 <= 3 (Met)
    // J2: arr 0, burst 3, ddl 4 -> finish 5 > 4 (Missed)
    // J3: arr 0, burst 2, ddl 8 -> finish 7 <= 8 (Met)
    // J4: arr 0, burst 3, ddl 8 -> finish 10 > 8 (Missed)
    // J5: arr 0, burst 2, ddl -1 -> finish 12 (No deadline, excluded)
    sim.add_job(Job::make(1, JobClass::Inference, 0, 2, 0, 3, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 3, 0, 4, 0, "J2"));
    sim.add_job(Job::make(3, JobClass::Inference, 0, 2, 0, 8, 0, "J3"));
    sim.add_job(Job::make(4, JobClass::Inference, 0, 3, 0, 8, 0, "J4"));
    sim.add_job(Job::make(5, JobClass::Training,  0, 2, 0, -1, 0, "J5"));
    sim.run(100);

    auto m = mlsched::Metrics::calculate(sim, "FCFS");
    const auto& agg = m.aggregate();

    CHECK(agg.total_jobs == 5);
    CHECK(agg.deadline_count == 4);
    CHECK(agg.deadline_miss_count == 2);

    int miss_rate_pct = static_cast<int>(std::round(agg.deadline_miss_rate * 100.0));
    CHECK(miss_rate_pct == 50);

    // Inference class: 4 jobs with deadline, 2 misses
    const auto& infer = m.class_metrics(JobClass::Inference);
    CHECK(infer.deadline_count == 4);
    CHECK(infer.deadline_miss_count == 2);
    int infer_pct = static_cast<int>(std::round(infer.deadline_miss_rate * 100.0));
    CHECK(infer_pct == 50);

    // Training class: 1 job, 0 with deadline
    const auto& train = m.class_metrics(JobClass::Training);
    CHECK(train.deadline_count == 0);
    CHECK(train.deadline_miss_count == 0);
    CHECK(train.deadline_miss_rate == 0.0);
}

// ---------------------------------------------------------------------------
//  Test 2: Throughput (completed jobs / makespan)
//  J1: burst 4 (finish 4)
//  J2: burst 6 (finish 10)
//  J3: burst 5 (finish 15)
//  Makespan = 15, completed = 3 -> throughput = 3 / 15 = 0.20
// ---------------------------------------------------------------------------
static void test_throughput() {
    std::cout << "  Running test_throughput...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));

    sim.add_job(Job::make(1, JobClass::Training, 0, 4, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 6, 0, -1, 0, "J2"));
    sim.add_job(Job::make(3, JobClass::Training, 2, 5, 0, -1, 0, "J3"));
    sim.run(100);

    auto m = mlsched::Metrics::calculate(sim, "FCFS");
    const auto& agg = m.aggregate();

    CHECK(agg.total_jobs == 3);
    CHECK(agg.makespan == 15);

    int throughput_x100 = static_cast<int>(std::round(agg.throughput * 100.0));
    CHECK(throughput_x100 == 20); // 0.20
}

// ---------------------------------------------------------------------------
//  Test 3: Starvation Metric
//  J1(Train): arr 0, burst 10 -> finish 10, wait 0
//  J2(Infer): arr 1, burst 2  -> finish 12, wait 9
//  J3(Infer): arr 2, burst 2  -> finish 14, wait 10
//  J4(Preproc): arr 3, burst 3 -> finish 17, wait 11
//  Threshold = 5: starved = 3 (J2, J3, J4)
//  Threshold = 9: starved = 2 (J3, J4)
// ---------------------------------------------------------------------------
static void test_starvation() {
    std::cout << "  Running test_starvation...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::Fcfs());
    mlsched::Simulator sim(std::move(sched));

    sim.add_job(Job::make(1, JobClass::Training,      0, 10, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference,     1,  2, 0, -1, 0, "J2"));
    sim.add_job(Job::make(3, JobClass::Inference,     2,  2, 0, -1, 0, "J3"));
    sim.add_job(Job::make(4, JobClass::Preprocessing, 3,  3, 0, -1, 0, "J4"));
    sim.run(100);

    // Evaluate with threshold = 5
    auto m5 = mlsched::Metrics::calculate(sim, "FCFS", 5);
    const auto& agg5 = m5.aggregate();

    CHECK(agg5.max_waiting_time == 11);
    CHECK(agg5.starved_count == 3); // J2(9), J3(10), J4(11)

    CHECK(m5.class_metrics(JobClass::Training).max_waiting_time == 0);
    CHECK(m5.class_metrics(JobClass::Training).starved_count == 0);

    CHECK(m5.class_metrics(JobClass::Inference).max_waiting_time == 10);
    CHECK(m5.class_metrics(JobClass::Inference).starved_count == 2);

    CHECK(m5.class_metrics(JobClass::Preprocessing).max_waiting_time == 11);
    CHECK(m5.class_metrics(JobClass::Preprocessing).starved_count == 1);

    // Evaluate with threshold = 9
    auto m9 = mlsched::Metrics::calculate(sim, "FCFS", 9);
    const auto& agg9 = m9.aggregate();

    CHECK(agg9.starved_count == 2); // J3(10), J4(11)
    CHECK(m9.class_metrics(JobClass::Inference).starved_count == 1);
    CHECK(m9.class_metrics(JobClass::Preprocessing).starved_count == 1);
}

// ---------------------------------------------------------------------------
//  Test 4: Timeline Segment Merging
//  RR (quantum 2): J1(burst 3), J2(burst 3)
//  Segments: J1 [0, 2), J2 [2, 4), J1 [4, 5), J2 [5, 6)
// ---------------------------------------------------------------------------
static void test_timeline_segments() {
    std::cout << "  Running test_timeline_segments...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));

    sim.add_job(Job::make(1, JobClass::Training, 0, 3, 0, -1, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Training, 0, 3, 0, -1, 0, "J2"));
    sim.run(100);

    auto m = mlsched::Metrics::calculate(sim, "RR");
    const auto& segs = m.segments();

    CHECK(segs.size() == 4);
    if (segs.size() == 4) {
        CHECK(segs[0].job_id == 1 && segs[0].start_tick == 0 && segs[0].end_tick == 2);
        CHECK(segs[1].job_id == 2 && segs[1].start_tick == 2 && segs[1].end_tick == 4);
        CHECK(segs[2].job_id == 1 && segs[2].start_tick == 4 && segs[2].end_tick == 5);
        CHECK(segs[3].job_id == 2 && segs[3].start_tick == 5 && segs[3].end_tick == 6);
    }
}

// ---------------------------------------------------------------------------
//  Test 5: CSV Exports
// ---------------------------------------------------------------------------
static void test_csv_exports() {
    std::cout << "  Running test_csv_exports...\n";
    using mlsched::Job;
    using mlsched::JobClass;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::RoundRobin(2));
    mlsched::Simulator sim(std::move(sched));

    sim.add_job(Job::make(1, JobClass::Training,  0, 2, 0,  5, 0, "J1"));
    sim.add_job(Job::make(2, JobClass::Inference, 0, 3, 0,  3, 0, "J2"));
    sim.run(100);

    auto m = mlsched::Metrics::calculate(sim, "RoundRobin_Test");
    bool exported = m.export_all("results");
    CHECK(exported == true);

    // Verify files exist and have non-zero size
    auto check_file = [](const std::string& path) {
        std::ifstream f(path);
        CHECK(f.good());
        if (f.good()) {
            std::string line;
            CHECK(static_cast<bool>(std::getline(f, line)));
        }
    };

    check_file("results/results.csv");
    check_file("results/per_job.csv");
    check_file("results/timeline.csv");
}

int main() {
    std::cout << "\n=== test_metrics ===\n\n";

    test_deadline_miss_rate();
    test_throughput();
    test_starvation();
    test_timeline_segments();
    test_csv_exports();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All Metrics tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
