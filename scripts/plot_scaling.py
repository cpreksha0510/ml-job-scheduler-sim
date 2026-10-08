#!/usr/bin/env python3
"""
plot_scaling.py  --  Log-log scaling plot of simulation runtime vs workload size.
Generates: docs/figures/scaling.png
"""

import os
import pandas as pd
import matplotlib.pyplot as plt

def main():
    csv_path = os.path.join("docs", "scaling.csv")
    if not os.path.exists(csv_path):
        csv_path = os.path.join("results", "scaling.csv")

    if not os.path.exists(csv_path):
        print(f"Error: Could not find {csv_path}")
        return

    df = pd.read_csv(csv_path)

    # Filter to medium workload for constant-load scaling comparison
    df_med = df[df["workload"] == "medium"].copy()

    # Style configuration
    plt.style.use("seaborn-v0_8-whitegrid" if "seaborn-v0_8-whitegrid" in plt.style.available else "default")
    fig, ax = plt.subplots(figsize=(9, 6), dpi=300)

    # Distinct palette and markers for the 7 policies
    palette = {
        "FCFS":   ("#4A5568", "o", "-"),
        "SJF":    ("#3182CE", "s", "-"),
        "SRTF":   ("#00B4D8", "^", "-"),
        "RR":     ("#38A169", "D", "-"),
        "MLFQ":   ("#D69E2E", "v", "-"),
        "EDF":    ("#E53E3E", "p", "-"),
        "Hybrid": ("#805AD5", "P", "-"),
    }

    policies = ["FCFS", "SJF", "SRTF", "RR", "MLFQ", "EDF", "Hybrid"]

    for pol in policies:
        pdf = df_med[df_med["policy"] == pol].sort_values("actual_jobs")
        if pdf.empty:
            continue
        color, marker, linestyle = palette.get(pol, ("#000000", "o", "-"))
        ax.plot(
            pdf["actual_jobs"],
            pdf["median_sec"],
            label=pol,
            color=color,
            marker=marker,
            markersize=7,
            linewidth=2.0,
            linestyle=linestyle,
            alpha=0.9,
        )

    # Also plot the overloaded 10k data points as distinct hollow markers to show queue degradation
    df_ov = df[df["workload"] == "overloaded"]
    for _, row in df_ov.iterrows():
        pol = row["policy"]
        color, _, _ = palette.get(pol, ("#000000", "o", "-"))
        ax.scatter(
            row["actual_jobs"],
            row["median_sec"],
            facecolors="none",
            edgecolors=color,
            s=90,
            linewidths=2.0,
            zorder=5,
        )

    # Add annotation for overloaded comparison
    ax.annotate(
        "Overloaded (10k jobs)\nHigh queue backlog",
        xy=(10042, 0.258),
        xytext=(16000, 0.15),
        arrowprops=dict(facecolor="#4A5568", shrink=0.08, width=1.0, headwidth=6),
        fontsize=9,
        fontweight="bold",
        color="#2D3748",
        bbox=dict(boxstyle="round,pad=0.4", fc="#EDF2F7", ec="#CBD5E0", lw=1),
    )

    ax.set_xscale("log")
    ax.set_yscale("log")

    ax.set_title("Simulator Runtime Scaling by Scheduling Policy (Log-Log)", fontsize=14, fontweight="bold", pad=12)
    ax.set_xlabel("Number of Completed Jobs (log scale)", fontsize=11, fontweight="bold")
    ax.set_ylabel("Simulation Wall-Clock Time (seconds, log scale)", fontsize=11, fontweight="bold")

    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
    ax.legend(title="Policy", fontsize=10, title_fontsize=10, frameon=True, loc="upper left")

    # Add explanatory footnote
    fig.text(
        0.5, 0.01,
        "Filled markers: Medium constant load (~80% demand). Hollow rings: Overloaded workload (10k jobs).",
        ha="center", fontsize=8.5, color="#718096"
    )

    out_dir = os.path.join("docs", "figures")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "scaling.png")
    fig.tight_layout(rect=[0, 0.03, 1, 1])
    fig.savefig(out_path, dpi=300)
    plt.close(fig)
    print(f"[SUCCESS] Saved scaling plot to {out_path}")

if __name__ == "__main__":
    main()
