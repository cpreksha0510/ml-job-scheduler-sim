// ============================================================================
//  test_hybrid.cpp  –  Unit tests for the Hybrid scheduling policy
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/simulator.cpp src/metrics.cpp src/workload.cpp src/policies/hybrid.cpp tests/test_hybrid.cpp
//        -o test_hybrid
// ============================================================================
#include "simulator.hpp"
#include "metrics.hpp"
#include "workload.hpp"
#include "policies/hybrid.hpp"

#include <cassert>
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
//  Test 1: Reserved slot (K=4 vs K=0)
//  I1 = inference (0, 6, 50), T = training (0, 3, -1)
//  K=4: I1 finishes at 7 (waiting 1), T finishes at 9 (response 3, waiting 6)
//  K=0: I1 finishes at 6, T finishes at 9 (response 6)
// ---------------------------------------------------------------------------
static void test_hybrid_reserved_slot() {
    std::cout << "  Running test_hybrid_reserved_slot...\n";
    using mlsched::Job;
    using mlsched::JobClass;
    using mlsched::HybridConfig;
    using mlsched::HybridScheduler;

    // Sub-case 1: K = 4
    {
        HybridConfig cfg;
        cfg.tier1_quantum   = 8;
        cfg.tier2_quantum   = 32;
        cfg.aging_threshold = 1000;
        cfg.reserve_period  = 4;

        auto sched = std::unique_ptr<mlsched::Scheduler>(new HybridScheduler(cfg));
        mlsched::Simulator sim(std::move(sched));
        sim.add_job(Job::make(1, JobClass::Inference, 0, 6, 0, 50, 0, "I1"));
        sim.add_job(Job::make(2, JobClass::Training,  0, 3, 0, -1, 0, "T"));
        sim.run(100);

        auto metrics = mlsched::Metrics::calculate(sim);
        CHECK(metrics.job_metrics().size() == 2);
        if (metrics.job_metrics().size() == 2) {
            const auto& m_i1 = metrics.job_metric(1);
            const auto& m_t  = metrics.job_metric(2);

            CHECK(m_i1.finish_time == 7);
            CHECK(m_i1.waiting_time == 1);

            CHECK(m_t.finish_time == 9);
            CHECK(m_t.response_time == 3);
            CHECK(m_t.waiting_time == 6);
        }
    }

    // Sub-case 2: K = 0 (disabled reserved slot)
    {
        HybridConfig cfg;
        cfg.tier1_quantum   = 8;
        cfg.tier2_quantum   = 32;
        cfg.aging_threshold = 1000;
        cfg.reserve_period  = 0;

        auto sched = std::unique_ptr<mlsched::Scheduler>(new HybridScheduler(cfg));
        mlsched::Simulator sim(std::move(sched));
        sim.add_job(Job::make(1, JobClass::Inference, 0, 6, 0, 50, 0, "I1"));
        sim.add_job(Job::make(2, JobClass::Training,  0, 3, 0, -1, 0, "T"));
        sim.run(100);

        auto metrics = mlsched::Metrics::calculate(sim);
        CHECK(metrics.job_metrics().size() == 2);
        if (metrics.job_metrics().size() == 2) {
            const auto& m_i1 = metrics.job_metric(1);
            const auto& m_t  = metrics.job_metric(2);

            CHECK(m_i1.finish_time == 6);
            CHECK(m_t.finish_time == 9);
            CHECK(m_t.response_time == 6);
        }
    }
}

// ---------------------------------------------------------------------------
//  Test 2: EDF preemption in Tier 0 (K=0)
//  A = (0, 4, 10), B = (1, 2, 5), both inference.
//  B finishes at 3 (waiting 0). A runs 0-1 then 3-6, finishes at 6 (waiting 2).
// ---------------------------------------------------------------------------
static void test_hybrid_edf_preemption() {
    std::cout << "  Running test_hybrid_edf_preemption...\n";
    using mlsched::Job;
    using mlsched::JobClass;
    using mlsched::HybridConfig;
    using mlsched::HybridScheduler;

    HybridConfig cfg;
    cfg.tier1_quantum   = 8;
    cfg.tier2_quantum   = 32;
    cfg.aging_threshold = 1000;
    cfg.reserve_period  = 0;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new HybridScheduler(cfg));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference, 0, 4, 0, 10, 0, "A"));
    sim.add_job(Job::make(2, JobClass::Inference, 1, 2, 0, 5,  0, "B"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 2);
    if (metrics.job_metrics().size() == 2) {
        const auto& mA = metrics.job_metric(1);
        const auto& mB = metrics.job_metric(2);

        CHECK(mB.finish_time == 3);
        CHECK(mB.waiting_time == 0);

        CHECK(mA.finish_time == 6);
        CHECK(mA.waiting_time == 2);
    }

    const auto& segs = sim.execution_segments();
    CHECK(segs.size() == 3);
    if (segs.size() == 3) {
        CHECK(segs[0].job_id == 1 && segs[0].start_tick == 0 && segs[0].end_tick == 1);
        CHECK(segs[1].job_id == 2 && segs[1].start_tick == 1 && segs[1].end_tick == 3);
        CHECK(segs[2].job_id == 1 && segs[2].start_tick == 3 && segs[2].end_tick == 6);
    }
}

// ---------------------------------------------------------------------------
//  Test 3: Tier ordering and inference preemption of training
//  Training and preprocessing arrive at t=0: preprocessing runs first.
//  Inference arrival at t=2 preempts running training job; training keeps tier.
// ---------------------------------------------------------------------------
static void test_hybrid_tier_ordering() {
    std::cout << "  Running test_hybrid_tier_ordering...\n";
    using mlsched::Job;
    using mlsched::JobClass;
    using mlsched::HybridConfig;
    using mlsched::HybridScheduler;

    HybridConfig cfg;
    cfg.tier1_quantum   = 8;
    cfg.tier2_quantum   = 32;
    cfg.aging_threshold = 1000;
    cfg.reserve_period  = 0;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new HybridScheduler(cfg));
    mlsched::Simulator sim(std::move(sched));
    // P arrives at 0, T arrives at 0 -> P runs first
    sim.add_job(Job::make(1, JobClass::Training,      0, 5, 0, -1, 0, "Train"));
    sim.add_job(Job::make(2, JobClass::Preprocessing, 0, 2, 0, -1, 0, "Preproc"));
    // Inference arrives at 3 while Train is running (at t=2..3)
    sim.add_job(Job::make(3, JobClass::Inference,     3, 2, 0, 10, 0, "Infer"));
    sim.run(100);

    // Segments expected:
    // t=0..2: Preproc (burst 2) finishes at 2
    // t=2..3: Train runs 1 tick
    // t=3..5: Infer preempts Train, runs 2 ticks, finishes at 5
    // t=5..9: Train resumes, runs remaining 4 ticks, finishes at 9
    const auto& segs = sim.execution_segments();
    CHECK(segs.size() == 4);
    if (segs.size() == 4) {
        CHECK(segs[0].job_id == 2 && segs[0].start_tick == 0 && segs[0].end_tick == 2);
        CHECK(segs[1].job_id == 1 && segs[1].start_tick == 2 && segs[1].end_tick == 3);
        CHECK(segs[2].job_id == 3 && segs[2].start_tick == 3 && segs[2].end_tick == 5);
        CHECK(segs[3].job_id == 1 && segs[3].start_tick == 5 && segs[3].end_tick == 9);
    }
}

// ---------------------------------------------------------------------------
//  Test 4: Aging (aging_threshold = 5, K = 0)
//  I1(0, 5, 50), T1(0, 2, -1), P1(5, 3, -1).
//  Assert:
//    I1 finishes at 5 (waiting 0)
//    T1 finishes at 7 (waiting 5, turnaround 7, response 5)
//    P1 finishes at 10 (waiting 2, turnaround 5, response 2)
// ---------------------------------------------------------------------------
static void test_hybrid_aging() {
    std::cout << "  Running test_hybrid_aging...\n";
    using mlsched::Job;
    using mlsched::JobClass;
    using mlsched::HybridConfig;
    using mlsched::HybridScheduler;

    HybridConfig cfg;
    cfg.tier1_quantum   = 8;
    cfg.tier2_quantum   = 32;
    cfg.aging_threshold = 5;
    cfg.reserve_period  = 0;

    auto sched = std::unique_ptr<mlsched::Scheduler>(new HybridScheduler(cfg));
    mlsched::Simulator sim(std::move(sched));
    sim.add_job(Job::make(1, JobClass::Inference,     0, 5, 0, 50, 0, "I1"));
    sim.add_job(Job::make(2, JobClass::Training,      0, 2, 0, -1, 0, "T1"));
    sim.add_job(Job::make(3, JobClass::Preprocessing, 5, 3, 0, -1, 0, "P1"));
    sim.run(100);

    auto metrics = mlsched::Metrics::calculate(sim);
    CHECK(metrics.job_metrics().size() == 3);
    if (metrics.job_metrics().size() == 3) {
        const auto& m_i1 = metrics.job_metric(1);
        const auto& m_t1 = metrics.job_metric(2);
        const auto& m_p1 = metrics.job_metric(3);

        CHECK(m_i1.finish_time == 5);
        CHECK(m_i1.waiting_time == 0);
        CHECK(m_i1.turnaround_time == 5);

        CHECK(m_t1.finish_time == 7);
        CHECK(m_t1.waiting_time == 5);
        CHECK(m_t1.turnaround_time == 7);
        CHECK(m_t1.response_time == 5);

        CHECK(m_p1.finish_time == 10);
        CHECK(m_p1.waiting_time == 2);
        CHECK(m_p1.turnaround_time == 5);
        CHECK(m_p1.response_time == 2);
    }
}

// ---------------------------------------------------------------------------
//  Test 5: Invariants on generated workloads
// ---------------------------------------------------------------------------
static void verify_workload_invariants(const std::string& cfg_path, const mlsched::HybridConfig& hcfg) {
    auto wcfg = mlsched::WorkloadConfig::load_from_file(cfg_path);
    wcfg.seed = 42; // deterministic seed
    mlsched::WorkloadGenerator gen(wcfg);
    auto jobs = gen.generate();

    int64_t total_burst = 0;
    for (const auto& j : jobs) {
        total_burst += j.burst;
    }

    auto sched = std::unique_ptr<mlsched::Scheduler>(new mlsched::HybridScheduler(hcfg));
    mlsched::Simulator sim(std::move(sched));
    sim.set_jobs(jobs);
    int64_t ticks_run = sim.run(200000);

    CHECK(ticks_run > 0);
    CHECK(sim.incomplete_jobs().empty());
    CHECK(sim.completed_jobs().size() == jobs.size());

    int64_t total_busy = 0;
    for (const auto& ev : sim.timeline()) {
        if (!ev.is_idle) ++total_busy;
    }
    CHECK(total_busy == total_burst);

    for (const auto& cj : sim.completed_jobs()) {
        CHECK(cj.finish_time >= cj.arrival + cj.burst);
    }
}

static void test_hybrid_workload_invariants() {
    std::cout << "  Running test_hybrid_workload_invariants...\n";
    mlsched::HybridConfig default_cfg; // default knobs

    // Medium config
    verify_workload_invariants("data/workload_medium.cfg", default_cfg);

    // Overloaded config
    verify_workload_invariants("data/workload_overloaded.cfg", default_cfg);

    // Also run with K=0 and aging very large
    mlsched::HybridConfig no_reserve_no_aging;
    no_reserve_no_aging.tier1_quantum   = 8;
    no_reserve_no_aging.tier2_quantum   = 32;
    no_reserve_no_aging.aging_threshold = 1000000;
    no_reserve_no_aging.reserve_period  = 0;

    verify_workload_invariants("data/workload_medium.cfg", no_reserve_no_aging);
    verify_workload_invariants("data/workload_overloaded.cfg", no_reserve_no_aging);
}

int main() {
    std::cout << "\n=== test_hybrid ===\n\n";

    test_hybrid_reserved_slot();
    test_hybrid_edf_preemption();
    test_hybrid_tier_ordering();
    test_hybrid_aging();
    test_hybrid_workload_invariants();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All Hybrid tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
