#!/usr/bin/env python3
"""
scripts/plot.py
Generates benchmark evaluation plots and summary tables for the ML Job Scheduler Simulator.
Reads only CSV files produced by experiments.exe.
"""

import os
import shutil
import csv
import matplotlib.pyplot as plt
import numpy as np

# Styling configuration
plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
plt.rcParams['font.sans-serif'] = 'DejaVu Sans', 'Arial', 'Helvetica'
plt.rcParams['axes.edgecolor'] = '#cccccc'
plt.rcParams['axes.linewidth'] = 0.8

DOCS_FIG_DIR = os.path.join("docs", "figures")
os.makedirs(DOCS_FIG_DIR, exist_ok=True)

POLICIES = ["FCFS", "SJF", "SRTF", "RR-small", "RR-large", "MLFQ", "EDF", "Hybrid"]
LOADS = ["light", "medium", "overloaded"]

POLICY_COLORS = {
    "FCFS": "#7f7f7f",
    "SJF": "#bcbd22",
    "SRTF": "#17becf",
    "RR-small": "#ff7f0e",
    "RR-large": "#d62728",
    "MLFQ": "#9467bd",
    "EDF": "#2ca02c",
    "Hybrid": "#1f77b4"
}

CLASS_COLORS = {
    "TRAIN": "#9467bd",
    "INFER": "#1f77b4",
    "PREPROC": "#ff7f0e"
}

def load_summary_csv(path="results/summary.csv"):
    """
    Returns dict: (policy, load, class, metric) -> {'mean': float, 'std': float, 'n': int}
    """
    data = {}
    with open(path, mode='r', newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row['policy'], row['load'], row['class'], row['metric'])
            data[key] = {
                'mean': float(row['mean']),
                'std': float(row['std']),
                'n': int(row['n'])
            }
    return data


# ============================================================================
# Plot 1: Inference Deadline-Miss Rate vs Load
# ============================================================================
def plot_deadline_miss_rate(summary):
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=300)

    x = np.arange(len(LOADS))
    width = 0.10
    num_policies = len(POLICIES)

    for i, policy in enumerate(POLICIES):
        means = []
        stds = []
        for load in LOADS:
            item = summary.get((policy, load, "inference", "deadline_miss_rate"), {'mean': 0.0, 'std': 0.0})
            means.append(item['mean'])
            stds.append(item['std'])

        offset = (i - num_policies / 2.0 + 0.5) * width
        rects = ax.bar(
            x + offset, means, width, yerr=stds, capsize=3,
            label=policy, color=POLICY_COLORS.get(policy, "#333333"),
            edgecolor='black', linewidth=0.5, alpha=0.9
        )

    ax.set_ylabel("Inference Deadline Miss Rate (0.0 to 1.0)", fontsize=11, fontweight='bold')
    ax.set_title("Inference Deadline-Miss Rate vs Workload Load (Mean ± Std, 10 Seeds)", fontsize=12, fontweight='bold', pad=12)
    ax.set_xticks(x)
    ax.set_xticklabels([l.capitalize() for l in LOADS], fontsize=11, fontweight='bold')
    ax.set_ylim(-0.02, 1.05)
    ax.legend(title="Policy", bbox_to_anchor=(1.02, 1), loc='upper left', frameon=True)

    plt.tight_layout()
    out_path = os.path.join(DOCS_FIG_DIR, "fig1_inference_deadline_miss_rate.png")
    plt.savefig(out_path)
    plt.close()
    print(f"[Plot 1] Saved {out_path}")


# ============================================================================
# Plot 2: Average Waiting Time per Class per Policy for Each Load
# ============================================================================
def plot_waiting_time_by_class(summary):
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.2), dpi=300, sharey=False)
    classes = ["inference", "preprocessing", "training"]

    for ax_idx, load in enumerate(LOADS):
        ax = axes[ax_idx]
        x = np.arange(len(classes))
        width = 0.10

        for p_idx, policy in enumerate(POLICIES):
            vals = []
            for cls in classes:
                item = summary.get((policy, load, cls, "avg_waiting_time"), {'mean': 0.0})
                vals.append(item['mean'])

            offset = (p_idx - len(POLICIES) / 2.0 + 0.5) * width
            ax.bar(
                x + offset, vals, width, label=policy if ax_idx == 0 else "",
                color=POLICY_COLORS.get(policy, "#333333"), edgecolor='black', linewidth=0.4
            )

        ax.set_title(f"Load: {load.capitalize()}", fontsize=11, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(["Inference", "Preprocessing", "Training"], fontsize=9, fontweight='bold')
        ax.set_ylabel("Avg Waiting Time (ticks)", fontsize=10)

    axes[0].legend(title="Policy", bbox_to_anchor=(0.5, -0.15), loc='upper center', ncol=4, frameon=True)
    fig.suptitle("Average Waiting Time by Job Class and Scheduling Policy", fontsize=13, fontweight='bold', y=0.98)
    plt.tight_layout()
    out_path = os.path.join(DOCS_FIG_DIR, "fig2_waiting_time_by_class.png")
    plt.savefig(out_path)
    plt.close()
    print(f"[Plot 2] Saved {out_path}")


# ============================================================================
# Plot 3: Training Starvation (Max Waiting Time of Training Jobs)
# ============================================================================
def plot_training_starvation(summary):
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=300)

    x = np.arange(len(LOADS))
    width = 0.10

    for i, policy in enumerate(POLICIES):
        vals = []
        errs = []
        for load in LOADS:
            item = summary.get((policy, load, "training", "max_waiting_time"), {'mean': 0.0, 'std': 0.0})
            vals.append(item['mean'])
            errs.append(item['std'])

        offset = (i - len(POLICIES) / 2.0 + 0.5) * width
        ax.bar(
            x + offset, vals, width, yerr=errs, capsize=3,
            label=policy, color=POLICY_COLORS.get(policy, "#333333"),
            edgecolor='black', linewidth=0.5
        )

    ax.set_ylabel("Max Waiting Time (ticks)", fontsize=11, fontweight='bold')
    ax.set_title("Training Starvation: Max Waiting Time of Training Jobs (Mean ± Std)", fontsize=12, fontweight='bold', pad=12)
    ax.set_xticks(x)
    ax.set_xticklabels([l.capitalize() for l in LOADS], fontsize=11, fontweight='bold')
    ax.legend(title="Policy", bbox_to_anchor=(1.02, 1), loc='upper left', frameon=True)

    plt.tight_layout()
    out_path = os.path.join(DOCS_FIG_DIR, "fig3_training_starvation.png")
    plt.savefig(out_path)
    plt.close()
    print(f"[Plot 3] Saved {out_path}")


# ============================================================================
# Plot 4: Throughput per Policy per Load
# ============================================================================
def plot_throughput(summary):
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=300)

    x = np.arange(len(LOADS))
    width = 0.10

    for i, policy in enumerate(POLICIES):
        vals = []
        for load in LOADS:
            item = summary.get((policy, load, "overall", "throughput"), {'mean': 0.0})
            vals.append(item['mean'])

        offset = (i - len(POLICIES) / 2.0 + 0.5) * width
        ax.bar(
            x + offset, vals, width,
            label=policy, color=POLICY_COLORS.get(policy, "#333333"),
            edgecolor='black', linewidth=0.5
        )

    ax.set_ylabel("Throughput (jobs completed / tick)", fontsize=11, fontweight='bold')
    ax.set_title("System Throughput per Policy and Workload Load", fontsize=12, fontweight='bold', pad=12)
    ax.set_xticks(x)
    ax.set_xticklabels([l.capitalize() for l in LOADS], fontsize=11, fontweight='bold')
    ax.legend(title="Policy", bbox_to_anchor=(1.02, 1), loc='upper left', frameon=True)

    plt.tight_layout()
    out_path = os.path.join(DOCS_FIG_DIR, "fig4_throughput.png")
    plt.savefig(out_path)
    plt.close()
    print(f"[Plot 4] Saved {out_path}")


# ============================================================================
# Plot 5: Gantt Chart for Hybrid vs RR-small (Light Load ~30 jobs)
# ============================================================================
def load_timeline_csv(filepath):
    segments = []
    if not os.path.exists(filepath):
        return segments
    with open(filepath, mode='r', newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            segments.append({
                'job_id': int(row['job_id']),
                'class': row['class'].strip().upper(),
                'start': int(row['start']),
                'end': int(row['end'])
            })
    return segments

def plot_gantt_chart():
    hybrid_segs = load_timeline_csv("results/timeline_hybrid.csv")
    rr_segs     = load_timeline_csv("results/timeline_rr.csv")

    if not hybrid_segs or not rr_segs:
        print("[Plot 5] Warning: timeline CSVs missing, skipping Gantt chart.")
        return

    fig, (ax_hyb, ax_rr) = plt.subplots(2, 1, figsize=(14, 6), sharex=True, dpi=300)

    # Plot Hybrid
    for seg in hybrid_segs:
        c = CLASS_COLORS.get(seg['class'], "#7f7f7f")
        ax_hyb.barh(y=seg['job_id'], width=seg['end'] - seg['start'], left=seg['start'],
                    height=0.8, color=c, edgecolor='black', linewidth=0.3)

    ax_hyb.set_title("Hybrid Scheduler (Tier 0 EDF / Tier 1 Preproc / Tier 2 Train + Reserved Slot)",
                     fontsize=11, fontweight='bold')
    ax_hyb.set_ylabel("Job ID", fontsize=10)
    ax_hyb.grid(True, linestyle='--', alpha=0.5)

    # Plot RR-small
    for seg in rr_segs:
        c = CLASS_COLORS.get(seg['class'], "#7f7f7f")
        ax_rr.barh(y=seg['job_id'], width=seg['end'] - seg['start'], left=seg['start'],
                   height=0.8, color=c, edgecolor='black', linewidth=0.3)

    ax_rr.set_title("Round Robin (Quantum = 2)", fontsize=11, fontweight='bold')
    ax_rr.set_ylabel("Job ID", fontsize=10)
    ax_rr.set_xlabel("Simulation Tick", fontsize=11, fontweight='bold')
    ax_rr.grid(True, linestyle='--', alpha=0.5)

    # Create custom legend for classes
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor=CLASS_COLORS['INFER'], edgecolor='black', label='Inference (Tier 0)'),
        Patch(facecolor=CLASS_COLORS['PREPROC'], edgecolor='black', label='Preprocessing (Tier 1)'),
        Patch(facecolor=CLASS_COLORS['TRAIN'], edgecolor='black', label='Training (Tier 2)')
    ]
    fig.legend(handles=legend_elements, loc='upper right', bbox_to_anchor=(0.98, 0.98), ncol=3, frameon=True)

    fig.suptitle("Timeline Execution Comparison (Light Workload ~30 Jobs)", fontsize=13, fontweight='bold', y=0.98)
    plt.tight_layout(rect=[0, 0, 1, 0.94])
    out_path = os.path.join(DOCS_FIG_DIR, "fig5_gantt_chart.png")
    plt.savefig(out_path)
    plt.close()
    print(f"[Plot 5] Saved {out_path}")


def main():
    summary_path = "results/summary.csv"
    if not os.path.exists(summary_path):
        print(f"Error: {summary_path} not found. Run experiments.exe first.")
        return

    summary = load_summary_csv(summary_path)

    plot_deadline_miss_rate(summary)
    plot_waiting_time_by_class(summary)
    plot_training_starvation(summary)
    plot_throughput(summary)
    plot_gantt_chart()

    # Copy summary.csv to docs/results_summary.csv
    docs_summary = os.path.join("docs", "results_summary.csv")
    shutil.copyfile(summary_path, docs_summary)
    print(f"Copied {summary_path} -> {docs_summary}")

if __name__ == "__main__":
    main()
