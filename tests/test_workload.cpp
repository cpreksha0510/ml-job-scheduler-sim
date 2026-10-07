// ============================================================================
//  test_workload.cpp  –  Unit tests for WorkloadGenerator & WorkloadConfig
//
//  Build:
//    g++ -std=c++14 -Wall -Wextra -Iinclude -Isrc
//        src/workload.cpp tests/test_workload.cpp -o test_workload
// ============================================================================
#include "workload.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
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
//  Test 1: Determinism (same seed = identical jobs, different seed = differs)
// ---------------------------------------------------------------------------
static void test_determinism() {
    std::cout << "  Running test_determinism...\n";

    mlsched::WorkloadConfig cfg1;
    cfg1.seed = 12345;
    cfg1.horizon = 1000;

    mlsched::WorkloadConfig cfg2 = cfg1; // identical

    mlsched::WorkloadConfig cfg3 = cfg1;
    cfg3.seed = 99999; // different seed

    mlsched::WorkloadGenerator gen1(cfg1);
    auto jobs1 = gen1.generate();

    mlsched::WorkloadGenerator gen2(cfg2);
    auto jobs2 = gen2.generate();

    mlsched::WorkloadGenerator gen3(cfg3);
    auto jobs3 = gen3.generate();

    // gen1 and gen2 must produce identical output
    CHECK(!jobs1.empty());
    CHECK(jobs1.size() == jobs2.size());
    if (jobs1.size() == jobs2.size()) {
        bool identical = true;
        for (size_t i = 0; i < jobs1.size(); ++i) {
            if (jobs1[i].id != jobs2[i].id ||
                jobs1[i].arrival != jobs2[i].arrival ||
                jobs1[i].burst != jobs2[i].burst ||
                jobs1[i].deadline != jobs2[i].deadline ||
                jobs1[i].priority != jobs2[i].priority ||
                jobs1[i].mem_req != jobs2[i].mem_req ||
                jobs1[i].job_class != jobs2[i].job_class)
            {
                identical = false;
                break;
            }
        }
        CHECK(identical);
    }

    // gen3 (different seed) must differ
    bool differs = (jobs1.size() != jobs3.size());
    if (!differs) {
        for (size_t i = 0; i < jobs1.size(); ++i) {
            if (jobs1[i].arrival != jobs3[i].arrival || jobs1[i].burst != jobs3[i].burst) {
                differs = true;
                break;
            }
        }
    }
    CHECK(differs);
}

// ---------------------------------------------------------------------------
//  Test 2: Job invariants
//  - every job has burst >= 1
//  - deadline >= arrival + burst when deadline is present
//  - jobs are sorted by arrival ascending
//  - sequential IDs 0..n-1
// ---------------------------------------------------------------------------
static void test_job_invariants() {
    std::cout << "  Running test_job_invariants...\n";

    std::vector<std::string> configs = {
        "data/workload_light.cfg",
        "data/workload_medium.cfg",
        "data/workload_overloaded.cfg"
    };

    for (const auto& path : configs) {
        auto cfg = mlsched::WorkloadConfig::load_from_file(path);
        mlsched::WorkloadGenerator gen(cfg);
        auto jobs = gen.generate();

        CHECK(!jobs.empty());

        for (size_t i = 0; i < jobs.size(); ++i) {
            const auto& j = jobs[i];

            // ID check
            CHECK(j.id == static_cast<uint32_t>(i));

            // Burst >= 1
            CHECK(j.burst >= 1);

            // Deadline >= arrival + burst if present
            if (j.deadline != mlsched::UNSET) {
                CHECK(j.deadline >= j.arrival + j.burst);
            }

            // Sorted by arrival
            if (i > 0) {
                CHECK(j.arrival >= jobs[i - 1].arrival);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Test 3: Long horizon inference mean inter-arrival within ~10% of 1/rate
// ---------------------------------------------------------------------------
static void test_inference_inter_arrival() {
    std::cout << "  Running test_inference_inter_arrival...\n";

    mlsched::WorkloadConfig cfg;
    cfg.seed = 42;
    cfg.horizon = 100000;     // 100k ticks
    cfg.infer_rate = 0.05;    // Expected mean inter-arrival = 1 / 0.05 = 20.0
    cfg.train_rate = 0.0;     // only inference
    cfg.preproc_rate = 0.0;

    mlsched::WorkloadGenerator gen(cfg);
    auto jobs = gen.generate();

    CHECK(jobs.size() > 1000);
    if (jobs.size() > 1) {
        double first_arr = static_cast<double>(jobs.front().arrival);
        double last_arr  = static_cast<double>(jobs.back().arrival);
        double mean_inter_arrival = (last_arr - first_arr) / static_cast<double>(jobs.size() - 1);

        double expected = 1.0 / cfg.infer_rate; // 20.0
        double error_ratio = std::abs(mean_inter_arrival - expected) / expected;

        // Within 10%
        CHECK(error_ratio <= 0.10);
    }
}

// ---------------------------------------------------------------------------
//  Test 4: Config parser handles comments, blank lines, and missing keys
// ---------------------------------------------------------------------------
static void test_config_parser() {
    std::cout << "  Running test_config_parser...\n";

    const std::string test_cfg_path = "data/test_custom_config.cfg";
    {
        std::ofstream out(test_cfg_path);
        out << "# This is a full-line comment\n"
            << "\n"
            << "   # Comment with leading whitespace\n"
            << "seed = 777\n"
            << "horizon = 5000\n"
            << "\n"
            << "# Missing infer_burst_min, train_rate, etc. should use defaults\n"
            << "infer_rate = 0.25\n"
            << "train_priority = 3\n";
    }

    auto cfg = mlsched::WorkloadConfig::load_from_file(test_cfg_path);

    // Explicitly provided values
    CHECK(cfg.seed == 777);
    CHECK(cfg.horizon == 5000);
    CHECK(std::abs(cfg.infer_rate - 0.25) < 1e-6);
    CHECK(cfg.train_priority == 3);

    // Fallback defaults for missing keys
    CHECK(cfg.infer_burst_min == 1);
    CHECK(cfg.infer_burst_max == 4);
    CHECK(cfg.preproc_batch_size == 4);

    std::remove(test_cfg_path.c_str());
}

int main() {
    std::cout << "\n=== test_workload ===\n\n";

    test_determinism();
    test_job_invariants();
    test_inference_inter_arrival();
    test_config_parser();

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "[PASS] All WorkloadGenerator tests passed.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << "[FAIL] " << g_failures << " assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
}
