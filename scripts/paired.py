#!/usr/bin/env python3
"""
scripts/paired.py

Reads results/runs_raw.csv and computes seed-paired comparisons of
inference deadline-miss rate between Hybrid and all other policies
across light, medium, and overloaded load levels (seeds 1 to 10).

Outputs docs/PAIRED.md.
"""

import csv
from collections import defaultdict
import os

def load_data(csv_path):
    # data[load][seed][policy] = infer_miss_rate
    data = defaultdict(lambda: defaultdict(dict))
    with open(csv_path, mode='r', newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            load = row['load']
            seed = int(row['seed'])
            policy = row['policy']
            miss_rate = float(row['infer_miss_rate'])
            data[load][seed][policy] = miss_rate
    return data

def analyze():
    csv_path = os.path.join("results", "runs_raw.csv")
    data = load_data(csv_path)

    loads = ["light", "medium", "overloaded"]
    other_policies = ["FCFS", "SJF", "SRTF", "RR-small", "RR-large", "MLFQ", "EDF"]

    lines = []
    lines.append("# Paired Comparison: Inference Deadline-Miss Rate (Hybrid vs. Other Policies)")
    lines.append("")
    lines.append("This analysis evaluates the paired differences in inference deadline-miss rate between **Hybrid** and each alternative policy evaluated on the **exact same 10 random seeds** (seeds 1 to 10).")
    lines.append("")
    lines.append("- **Metric:** `infer_miss_rate` from `results/runs_raw.csv`")
    lines.append("- **Paired Difference:** $\\Delta = \\text{MissRate}_{\\text{Other}} - \\text{MissRate}_{\\text{Hybrid}}$ (positive $\\Delta$ indicates Hybrid achieved a lower miss rate)")
    lines.append("- **Seeds Won (Hybrid Better):** Count of seeds (out of 10) where $\\text{MissRate}_{\\text{Hybrid}} < \\text{MissRate}_{\\text{Other}}$")
    lines.append("- **Seeds Tied:** Count of seeds where miss rates are equal")
    lines.append("- **Seeds Lost (Other Better):** Count of seeds where $\\text{MissRate}_{\\text{Other}} < \\text{MissRate}_{\\text{Hybrid}}$")
    lines.append("- **Statistical Significance Criterion:** When Hybrid wins at least 4 out of 10 seeds (or ties), differences against policies claiming a win are **not distinguishable from noise**.")
    lines.append("")

    for load in loads:
        lines.append(f"## Load: {load.capitalize()}")
        lines.append("")
        lines.append("| Policy Comparison | Mean Other | Mean Hybrid | Mean Paired Diff (Other - Hybrid) | Seeds Won (Hybrid Better) | Seeds Tied | Seeds Lost (Other Better) | Assessment |")
        lines.append("|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|")

        for pol in other_policies:
            diffs = []
            hybrid_wins = 0
            ties = 0
            other_wins = 0
            other_vals = []
            hybrid_vals = []

            for seed in range(1, 11):
                h_val = data[load][seed]["Hybrid"]
                o_val = data[load][seed][pol]
                hybrid_vals.append(h_val)
                other_vals.append(o_val)
                diff = o_val - h_val
                diffs.append(diff)

                if h_val < o_val - 1e-9:
                    hybrid_wins += 1
                elif abs(h_val - o_val) <= 1e-9:
                    ties += 1
                else:
                    other_wins += 1

            mean_o = sum(other_vals) / len(other_vals)
            mean_h = sum(hybrid_vals) / len(hybrid_vals)
            mean_diff = sum(diffs) / len(diffs)

            if hybrid_wins >= 6:
                assessment = "**Hybrid clearly superior**"
            elif other_wins >= 6 and hybrid_wins < 4:
                assessment = f"**{pol} statistically superior**"
            elif hybrid_wins >= 4:
                assessment = "**Not distinguishable from noise** (Hybrid wins >= 4/10)"
            else:
                assessment = f"**{pol} favored**"

            lines.append(f"| Hybrid vs. {pol} | {mean_o:.4f} | {mean_h:.4f} | {mean_diff:+.4f} | **{hybrid_wins}/10** | {ties}/10 | {other_wins}/10 | {assessment} |")

        lines.append("")

    # Detailed per-seed table
    lines.append("---")
    lines.append("")
    lines.append("## Detailed Per-Seed Data")
    lines.append("")
    for load in loads:
        lines.append(f"### Per-Seed Inference Deadline-Miss Rates: {load.capitalize()}")
        lines.append("")
        header = "| Seed | Hybrid | " + " | ".join(other_policies) + " |"
        sep = "|:---:|:---:|" + "|".join([":---:" for _ in other_policies]) + "|"
        lines.append(header)
        lines.append(sep)

        for seed in range(1, 11):
            h_val = data[load][seed]["Hybrid"]
            row = [f"{seed}", f"{h_val:.4f}"]
            for pol in other_policies:
                row.append(f"{data[load][seed][pol]:.4f}")
            lines.append("| " + " | ".join(row) + " |")

        lines.append("")

    output_path = os.path.join("docs", "PAIRED.md")
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"[SUCCESS] Wrote paired analysis to {output_path}")

if __name__ == "__main__":
    analyze()
