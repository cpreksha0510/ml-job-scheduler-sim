// ============================================================================
//  metrics.cpp  –  Metrics evaluation and CSV exporter implementation
//  C++14, STL only.
// ============================================================================
#include "metrics.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h>  // _mkdir
#else
#include <sys/stat.h> // mkdir
#endif

namespace mlsched {

static void ensure_directory_exists(const std::string& path) {
    std::string dir;
    size_t last_slash = path.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        dir = path.substr(0, last_slash);
    }
    if (!dir.empty()) {
#ifdef _WIN32
        _mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0755);
#endif
    }
}

// ---------------------------------------------------------------------------
Metrics Metrics::calculate(const Simulator& sim,
                           const std::string& policy_name,
                           int64_t starvation_threshold)
{
    Metrics m;
    m.agg_.policy_name = policy_name;
    m.agg_.starvation_threshold = starvation_threshold;
    m.agg_.training.job_class = JobClass::Training;
    m.agg_.inference.job_class = JobClass::Inference;
    m.agg_.preprocessing.job_class = JobClass::Preprocessing;

    m.segments_ = sim.execution_segments();

    const auto& completed = sim.completed_jobs();
    m.agg_.total_jobs = completed.size();

    if (completed.empty()) {
        return m;
    }

    double total_wait = 0.0, total_ta = 0.0, total_rt = 0.0;
    int64_t max_finish = 0;
    int64_t max_wait = 0;
    size_t starved_total = 0;
    size_t deadline_jobs_total = 0;
    size_t deadline_misses_total = 0;

    // Helper per-class accumulators
    struct ClassAcc {
        double total_wait = 0.0;
        double total_ta   = 0.0;
        double total_rt   = 0.0;
        int64_t max_wait  = 0;
        size_t count      = 0;
        size_t deadline_cnt = 0;
        size_t miss_cnt     = 0;
        size_t starved_cnt  = 0;
    };

    ClassAcc acc_train, acc_infer, acc_preproc;

    for (const auto& j : completed) {
        JobMetric jm;
        jm.job_id         = j.id;
        jm.label          = j.label;
        jm.job_class      = j.job_class;
        jm.arrival        = j.arrival;
        jm.burst          = j.burst;
        jm.deadline       = j.deadline;
        jm.start_time     = j.start_time;
        jm.finish_time    = j.finish_time;
        jm.first_run_time = j.first_run_time;

        jm.turnaround_time = (j.finish_time != UNSET) ? (j.finish_time - j.arrival) : 0;
        if (j.burst == 0) {
            jm.waiting_time = (j.start_time != UNSET) ? (j.start_time - j.arrival) : 0;
        } else {
            jm.waiting_time = jm.turnaround_time - j.burst;
        }
        jm.response_time   = (j.first_run_time != UNSET) ? (j.first_run_time - j.arrival) : 0;

        jm.has_deadline    = (j.deadline != UNSET);
        jm.missed_deadline = jm.has_deadline && (j.finish_time != UNSET) && (j.finish_time > j.deadline);
        jm.is_starved      = (jm.waiting_time > starvation_threshold);

        if (j.finish_time > max_finish) {
            max_finish = j.finish_time;
        }
        if (jm.waiting_time > max_wait) {
            max_wait = jm.waiting_time;
        }
        if (jm.is_starved) {
            ++starved_total;
        }

        total_wait += static_cast<double>(jm.waiting_time);
        total_ta   += static_cast<double>(jm.turnaround_time);
        total_rt   += static_cast<double>(jm.response_time);

        if (jm.has_deadline) {
            ++deadline_jobs_total;
            if (jm.missed_deadline) {
                ++deadline_misses_total;
            }
        }

        // Per-class accumulation
        ClassAcc* acc = nullptr;
        switch (j.job_class) {
            case JobClass::Training:      acc = &acc_train; break;
            case JobClass::Inference:     acc = &acc_infer; break;
            case JobClass::Preprocessing: acc = &acc_preproc; break;
        }

        if (acc) {
            ++acc->count;
            acc->total_wait += static_cast<double>(jm.waiting_time);
            acc->total_ta   += static_cast<double>(jm.turnaround_time);
            acc->total_rt   += static_cast<double>(jm.response_time);
            if (jm.waiting_time > acc->max_wait) {
                acc->max_wait = jm.waiting_time;
            }
            if (jm.has_deadline) {
                ++acc->deadline_cnt;
                if (jm.missed_deadline) {
                    ++acc->miss_cnt;
                }
            }
            if (jm.is_starved) {
                ++acc->starved_cnt;
            }
        }

        m.jobs_.push_back(jm);
    }

    m.agg_.makespan = max_finish;
    m.agg_.throughput = (max_finish > 0) ? (static_cast<double>(completed.size()) / static_cast<double>(max_finish)) : 0.0;

    double n = static_cast<double>(completed.size());
    m.agg_.avg_waiting_time    = total_wait / n;
    m.agg_.avg_turnaround_time = total_ta / n;
    m.agg_.avg_response_time   = total_rt / n;

    m.agg_.deadline_count      = deadline_jobs_total;
    m.agg_.deadline_miss_count = deadline_misses_total;
    m.agg_.deadline_miss_rate  = (deadline_jobs_total > 0)
        ? (static_cast<double>(deadline_misses_total) / static_cast<double>(deadline_jobs_total))
        : 0.0;

    m.agg_.max_waiting_time    = max_wait;
    m.agg_.starved_count       = starved_total;

    // Finalize per-class metrics
    auto finalize_class = [max_finish](ClassMetrics& cm, const ClassAcc& acc) {
        cm.job_count = acc.count;
        if (acc.count > 0) {
            double cn = static_cast<double>(acc.count);
            cm.avg_waiting_time    = acc.total_wait / cn;
            cm.avg_turnaround_time = acc.total_ta / cn;
            cm.avg_response_time   = acc.total_rt / cn;
            cm.max_waiting_time    = acc.max_wait;
            cm.starved_count       = acc.starved_cnt;
        }
        cm.deadline_count      = acc.deadline_cnt;
        cm.deadline_miss_count = acc.miss_cnt;
        cm.deadline_miss_rate  = (acc.deadline_cnt > 0)
            ? (static_cast<double>(acc.miss_cnt) / static_cast<double>(acc.deadline_cnt))
            : 0.0;
        cm.throughput = (max_finish > 0)
            ? (static_cast<double>(acc.count) / static_cast<double>(max_finish))
            : 0.0;
    };

    finalize_class(m.agg_.training, acc_train);
    finalize_class(m.agg_.inference, acc_infer);
    finalize_class(m.agg_.preprocessing, acc_preproc);

    return m;
}

// ---------------------------------------------------------------------------
const ClassMetrics& Metrics::class_metrics(JobClass c) const {
    switch (c) {
        case JobClass::Training:      return agg_.training;
        case JobClass::Inference:     return agg_.inference;
        case JobClass::Preprocessing: return agg_.preprocessing;
    }
    return agg_.training;
}

// ---------------------------------------------------------------------------
const JobMetric& Metrics::job_metric(uint32_t id) const {
    for (const auto& jm : jobs_) {
        if (jm.job_id == id) return jm;
    }
    throw std::out_of_range("Metrics::job_metric: Job ID not found");
}

// ---------------------------------------------------------------------------
bool Metrics::export_results_csv(const std::string& filepath) const {
    ensure_directory_exists(filepath);

    bool file_exists = false;
    {
        std::ifstream check(filepath);
        file_exists = check.good();
    }

    std::ofstream out(filepath, std::ios::app);
    if (!out.is_open()) return false;

    if (!file_exists) {
        out << "policy,total_jobs,makespan,throughput,avg_wait,avg_turnaround,avg_response,"
            << "deadline_count,deadline_miss_count,deadline_miss_rate,max_wait,starved_count,starvation_threshold,"
            << "train_jobs,train_avg_wait,train_avg_ta,train_avg_resp,train_miss_rate,train_max_wait,train_starved,"
            << "infer_jobs,infer_avg_wait,infer_avg_ta,infer_avg_resp,infer_miss_rate,infer_max_wait,infer_starved,"
            << "preproc_jobs,preproc_avg_wait,preproc_avg_ta,preproc_avg_resp,preproc_miss_rate,preproc_max_wait,preproc_starved\n";
    }

    out << (agg_.policy_name.empty() ? "Policy" : agg_.policy_name) << ","
        << agg_.total_jobs << ","
        << agg_.makespan << ","
        << agg_.throughput << ","
        << agg_.avg_waiting_time << ","
        << agg_.avg_turnaround_time << ","
        << agg_.avg_response_time << ","
        << agg_.deadline_count << ","
        << agg_.deadline_miss_count << ","
        << agg_.deadline_miss_rate << ","
        << agg_.max_waiting_time << ","
        << agg_.starved_count << ","
        << agg_.starvation_threshold << ","
        // Training
        << agg_.training.job_count << ","
        << agg_.training.avg_waiting_time << ","
        << agg_.training.avg_turnaround_time << ","
        << agg_.training.avg_response_time << ","
        << agg_.training.deadline_miss_rate << ","
        << agg_.training.max_waiting_time << ","
        << agg_.training.starved_count << ","
        // Inference
        << agg_.inference.job_count << ","
        << agg_.inference.avg_waiting_time << ","
        << agg_.inference.avg_turnaround_time << ","
        << agg_.inference.avg_response_time << ","
        << agg_.inference.deadline_miss_rate << ","
        << agg_.inference.max_waiting_time << ","
        << agg_.inference.starved_count << ","
        // Preprocessing
        << agg_.preprocessing.job_count << ","
        << agg_.preprocessing.avg_waiting_time << ","
        << agg_.preprocessing.avg_turnaround_time << ","
        << agg_.preprocessing.avg_response_time << ","
        << agg_.preprocessing.deadline_miss_rate << ","
        << agg_.preprocessing.max_waiting_time << ","
        << agg_.preprocessing.starved_count << "\n";

    return true;
}

// ---------------------------------------------------------------------------
bool Metrics::export_per_job_csv(const std::string& filepath) const {
    ensure_directory_exists(filepath);
    std::ofstream out(filepath, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "job_id,label,class,arrival,burst,deadline,start_time,finish_time,"
        << "waiting_time,turnaround_time,response_time,missed_deadline,is_starved\n";

    for (const auto& jm : jobs_) {
        out << jm.job_id << ","
            << jm.label << ","
            << to_string(jm.job_class) << ","
            << jm.arrival << ","
            << jm.burst << ","
            << jm.deadline << ","
            << jm.start_time << ","
            << jm.finish_time << ","
            << jm.waiting_time << ","
            << jm.turnaround_time << ","
            << jm.response_time << ","
            << (jm.missed_deadline ? "1" : "0") << ","
            << (jm.is_starved ? "1" : "0") << "\n";
    }

    return true;
}

// ---------------------------------------------------------------------------
bool Metrics::export_timeline_csv(const std::string& filepath) const {
    ensure_directory_exists(filepath);
    std::ofstream out(filepath, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "job_id,class,start,end\n";

    for (const auto& seg : segments_) {
        out << seg.job_id << ","
            << to_string(seg.job_class) << ","
            << seg.start_tick << ","
            << seg.end_tick << "\n";
    }

    return true;
}

// ---------------------------------------------------------------------------
bool Metrics::export_all(const std::string& output_dir) const {
    std::string prefix = output_dir;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') {
        prefix += "/";
    }
    bool ok1 = export_results_csv(prefix + "results.csv");
    bool ok2 = export_per_job_csv(prefix + "per_job.csv");
    bool ok3 = export_timeline_csv(prefix + "timeline.csv");
    return ok1 && ok2 && ok3;
}

} // namespace mlsched
