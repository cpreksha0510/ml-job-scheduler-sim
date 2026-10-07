#pragma once
// ============================================================================
//  csv_loader.hpp  –  Load a Job list from a CSV file
//  C++17, STL only.
//
//  Expected CSV columns (header line required):
//    id, class, arrival, burst, priority, deadline, mem_req, label
//
//  "class" must be one of: training, inference, preprocessing  (case-insensitive)
//  "deadline" may be empty or "-1" to mean UNSET.
//  "label"    may be empty; defaults to "J<id>".
// ============================================================================
#include "job.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

namespace mlsched {

namespace detail {

inline std::string trim(std::string s) {
    // leading
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [](unsigned char c){ return !std::isspace(c); }));
    // trailing
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c){ return !std::isspace(c); }).base(),
            s.end());
    return s;
}

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

inline JobClass parse_class(const std::string& raw) {
    auto l = lower(trim(raw));
    if (l == "training"  || l == "train")  return JobClass::Training;
    if (l == "inference" || l == "infer")  return JobClass::Inference;
    if (l == "preprocessing" || l == "preproc" || l == "prep")
        return JobClass::Preprocessing;
    throw std::runtime_error("Unknown job class: " + raw);
}

inline int64_t parse_optional_int(const std::string& raw, int64_t fallback = UNSET) {
    auto t = trim(raw);
    if (t.empty() || t == "-1") return fallback;
    return std::stoll(t);
}

/// Split a single CSV line respecting (basic) quoting.
inline std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;
    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field); // last field
    return fields;
}

} // namespace detail

/// Load jobs from a CSV file.  Throws std::runtime_error on parse errors.
inline std::vector<Job> load_jobs_from_csv(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open CSV: " + path);

    std::string line;
    // -- header ---------------------------------------------------------------
    if (!std::getline(f, line))
        throw std::runtime_error("CSV is empty: " + path);

    // Parse header to find column indices (flexible ordering).
    auto header_fields = detail::split_csv(line);
    auto col_idx = [&](const std::string& name) -> int {
        for (int i = 0; i < static_cast<int>(header_fields.size()); ++i) {
            if (detail::lower(detail::trim(header_fields[i])) == name)
                return i;
        }
        return -1;
    };

    int ci_id       = col_idx("id");
    int ci_class    = col_idx("class");
    int ci_arrival  = col_idx("arrival");
    int ci_burst    = col_idx("burst");
    int ci_priority = col_idx("priority");
    int ci_deadline = col_idx("deadline");
    int ci_mem_req  = col_idx("mem_req");
    int ci_label    = col_idx("label");

    if (ci_id < 0 || ci_class < 0 || ci_arrival < 0 || ci_burst < 0)
        throw std::runtime_error("CSV missing required columns: id, class, arrival, burst");

    // -- data rows ------------------------------------------------------------
    std::vector<Job> jobs;
    int row = 2;
    while (std::getline(f, line)) {
        if (detail::trim(line).empty()) { ++row; continue; }
        auto cols = detail::split_csv(line);

        auto get = [&](int ci) -> std::string {
            if (ci < 0 || ci >= static_cast<int>(cols.size())) return "";
            return detail::trim(cols[ci]);
        };

        try {
            uint32_t  id       = static_cast<uint32_t>(std::stoul(get(ci_id)));
            JobClass  cls      = detail::parse_class(get(ci_class));
            int64_t   arrival  = std::stoll(get(ci_arrival));
            int64_t   burst    = std::stoll(get(ci_burst));
            int32_t   priority = ci_priority >= 0 ? static_cast<int32_t>(std::stoi(get(ci_priority))) : 0;
            int64_t   deadline = detail::parse_optional_int(get(ci_deadline));
            int64_t   mem_req  = ci_mem_req >= 0 ? std::stoll(get(ci_mem_req)) : 0;
            std::string label  = get(ci_label);

            jobs.push_back(Job::make(id, cls, arrival, burst, priority,
                                     deadline, mem_req, label));
        } catch (const std::exception& e) {
            throw std::runtime_error(
                "CSV parse error on row " + std::to_string(row) + ": " + e.what());
        }
        ++row;
    }
    return jobs;
}

} // namespace mlsched
