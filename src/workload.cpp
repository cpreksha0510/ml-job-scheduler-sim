// ============================================================================
//  workload.cpp  –  Synthetic workload generator implementation
//  C++14, STL only.
// ============================================================================
#include "workload.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mlsched {

namespace {

inline std::string trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base(), s.end());
    return s;
}

inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

const char* class_to_csv_name(JobClass c) {
    switch (c) {
        case JobClass::Training:      return "training";
        case JobClass::Inference:     return "inference";
        case JobClass::Preprocessing: return "preprocessing";
    }
    return "training";
}

} // namespace

// ---------------------------------------------------------------------------
WorkloadConfig WorkloadConfig::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("WorkloadConfig: cannot open file " + path);
    }

    WorkloadConfig cfg;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue; // Skip comments and empty lines
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = to_lower(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));
        if (val.empty()) continue;

        try {
            if (key == "seed")                  cfg.seed = static_cast<uint32_t>(std::stoul(val));
            else if (key == "horizon")          cfg.horizon = std::stoll(val);
            // Inference
            else if (key == "infer_rate")       cfg.infer_rate = std::stod(val);
            else if (key == "infer_burst_min")  cfg.infer_burst_min = std::stoll(val);
            else if (key == "infer_burst_max")  cfg.infer_burst_max = std::stoll(val);
            else if (key == "infer_priority")   cfg.infer_priority = std::stoi(val);
            else if (key == "infer_slack")      cfg.infer_slack = std::stod(val);
            else if (key == "infer_mem_min")    cfg.infer_mem_min = std::stoll(val);
            else if (key == "infer_mem_max")    cfg.infer_mem_max = std::stoll(val);
            // Training
            else if (key == "train_rate")       cfg.train_rate = std::stod(val);
            else if (key == "train_burst_min")  cfg.train_burst_min = std::stoll(val);
            else if (key == "train_burst_max")  cfg.train_burst_max = std::stoll(val);
            else if (key == "train_priority")   cfg.train_priority = std::stoi(val);
            else if (key == "train_slack")      cfg.train_slack = std::stod(val);
            else if (key == "train_mem_min")    cfg.train_mem_min = std::stoll(val);
            else if (key == "train_mem_max")    cfg.train_mem_max = std::stoll(val);
            // Preprocessing
            else if (key == "preproc_rate")       cfg.preproc_rate = std::stod(val);
            else if (key == "preproc_batch_size") cfg.preproc_batch_size = std::stoi(val);
            else if (key == "preproc_burst_min")  cfg.preproc_burst_min = std::stoll(val);
            else if (key == "preproc_burst_max")  cfg.preproc_burst_max = std::stoll(val);
            else if (key == "preproc_priority")   cfg.preproc_priority = std::stoi(val);
            else if (key == "preproc_slack")      cfg.preproc_slack = std::stod(val);
            else if (key == "preproc_mem_min")    cfg.preproc_mem_min = std::stoll(val);
            else if (key == "preproc_mem_max")    cfg.preproc_mem_max = std::stoll(val);
        } catch (const std::exception& e) {
            throw std::runtime_error("WorkloadConfig: parse error for key '" + key + "': " + e.what());
        }
    }

    return cfg;
}

// ---------------------------------------------------------------------------
WorkloadGenerator::WorkloadGenerator(WorkloadConfig config)
    : config_(std::move(config))
    , rng_(config_.seed)
{
}

double WorkloadGenerator::uniform_real() {
    // Generate uniform double in (0.0, 1.0) strictly from 32-bit rng_ output
    return (static_cast<double>(rng_()) + 0.5) / 4294967296.0;
}

int64_t WorkloadGenerator::uniform_int(int64_t low, int64_t high) {
    if (low >= high) return low;
    uint64_t range = static_cast<uint64_t>(high - low + 1);
    return low + static_cast<int64_t>(rng_() % range);
}

double WorkloadGenerator::sample_exponential(double rate) {
    if (rate <= 0.0) return 1e9;
    double u = uniform_real();
    return -std::log(1.0 - u) / rate;
}

// ---------------------------------------------------------------------------
std::vector<Job> WorkloadGenerator::generate() {
    struct TaggedJob {
        Job     job;
        size_t  gen_order;
    };

    std::vector<TaggedJob> raw_jobs;
    size_t order_counter = 0;

    // 1. Inference jobs: Poisson arrivals (exponential inter-arrival times)
    if (config_.infer_rate > 0.0) {
        double current_t = 0.0;
        while (true) {
            current_t += sample_exponential(config_.infer_rate);
            int64_t arrival = static_cast<int64_t>(std::round(current_t));
            if (arrival >= config_.horizon) break;

            int64_t burst = std::max<int64_t>(1, uniform_int(config_.infer_burst_min, config_.infer_burst_max));
            int64_t deadline = UNSET;
            if (config_.infer_slack > 0.0) {
                int64_t allowance = static_cast<int64_t>(std::ceil(static_cast<double>(burst) * config_.infer_slack));
                deadline = arrival + std::max<int64_t>(burst, allowance);
            }
            int64_t mem = uniform_int(config_.infer_mem_min, config_.infer_mem_max);

            Job j = Job::make(0, JobClass::Inference, arrival, burst,
                              config_.infer_priority, deadline, mem, "Infer");
            raw_jobs.push_back({std::move(j), order_counter++});
        }
    }

    // 2. Training jobs: Sparse arrivals, long bursts
    if (config_.train_rate > 0.0) {
        double current_t = 0.0;
        while (true) {
            current_t += sample_exponential(config_.train_rate);
            int64_t arrival = static_cast<int64_t>(std::round(current_t));
            if (arrival >= config_.horizon) break;

            int64_t burst = std::max<int64_t>(1, uniform_int(config_.train_burst_min, config_.train_burst_max));
            int64_t deadline = UNSET;
            if (config_.train_slack > 0.0) {
                int64_t allowance = static_cast<int64_t>(std::ceil(static_cast<double>(burst) * config_.train_slack));
                deadline = arrival + std::max<int64_t>(burst, allowance);
            }
            int64_t mem = uniform_int(config_.train_mem_min, config_.train_mem_max);

            Job j = Job::make(0, JobClass::Training, arrival, burst,
                              config_.train_priority, deadline, mem, "Train");
            raw_jobs.push_back({std::move(j), order_counter++});
        }
    }

    // 3. Preprocessing jobs: Batchy arrivals (clusters of jobs arriving at the same tick)
    if (config_.preproc_rate > 0.0 && config_.preproc_batch_size > 0) {
        double current_t = 0.0;
        while (true) {
            current_t += sample_exponential(config_.preproc_rate);
            int64_t batch_arrival = static_cast<int64_t>(std::round(current_t));
            if (batch_arrival >= config_.horizon) break;

            for (int32_t b = 0; b < config_.preproc_batch_size; ++b) {
                int64_t burst = std::max<int64_t>(1, uniform_int(config_.preproc_burst_min, config_.preproc_burst_max));
                int64_t deadline = UNSET;
                if (config_.preproc_slack > 0.0) {
                    int64_t allowance = static_cast<int64_t>(std::ceil(static_cast<double>(burst) * config_.preproc_slack));
                    deadline = batch_arrival + std::max<int64_t>(burst, allowance);
                }
                int64_t mem = uniform_int(config_.preproc_mem_min, config_.preproc_mem_max);

                Job j = Job::make(0, JobClass::Preprocessing, batch_arrival, burst,
                                  config_.preproc_priority, deadline, mem, "Prep");
                raw_jobs.push_back({std::move(j), order_counter++});
            }
        }
    }

    // Sort jobs by arrival (ties by class, then generation order)
    std::sort(raw_jobs.begin(), raw_jobs.end(), [](const TaggedJob& a, const TaggedJob& b) {
        if (a.job.arrival != b.job.arrival) {
            return a.job.arrival < b.job.arrival;
        }
        if (a.job.job_class != b.job.job_class) {
            return static_cast<uint8_t>(a.job.job_class) < static_cast<uint8_t>(b.job.job_class);
        }
        return a.gen_order < b.gen_order;
    });

    // Assign IDs 0..n-1 sequentially
    std::vector<Job> result;
    result.reserve(raw_jobs.size());
    for (size_t i = 0; i < raw_jobs.size(); ++i) {
        Job j = raw_jobs[i].job;
        j.id = static_cast<uint32_t>(i);
        j.label = j.label + "-" + std::to_string(i);
        result.push_back(std::move(j));
    }

    return result;
}

// ---------------------------------------------------------------------------
WorkloadSummary WorkloadGenerator::summarize(const std::vector<Job>& jobs, int64_t horizon) {
    WorkloadSummary s;
    s.total_jobs = jobs.size();
    s.horizon = horizon;

    for (const auto& j : jobs) {
        s.total_demand += j.burst;
        switch (j.job_class) {
            case JobClass::Inference:
                ++s.infer_jobs;
                s.infer_demand += j.burst;
                break;
            case JobClass::Training:
                ++s.train_jobs;
                s.train_demand += j.burst;
                break;
            case JobClass::Preprocessing:
                ++s.preproc_jobs;
                s.preproc_demand += j.burst;
                break;
        }
    }

    s.demand_ratio = (horizon > 0) ? (static_cast<double>(s.total_demand) / static_cast<double>(horizon)) : 0.0;
    return s;
}

// ---------------------------------------------------------------------------
bool save_jobs_to_csv(const std::string& path, const std::vector<Job>& jobs) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "id,class,arrival,burst,priority,deadline,mem_req,label\n";

    for (const auto& j : jobs) {
        out << j.id << ","
            << class_to_csv_name(j.job_class) << ","
            << j.arrival << ","
            << j.burst << ","
            << j.priority << ","
            << j.deadline << ","
            << j.mem_req << ","
            << j.label << "\n";
    }

    return true;
}

} // namespace mlsched
