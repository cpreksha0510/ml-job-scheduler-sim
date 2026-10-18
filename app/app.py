#!/usr/bin/env python3
"""
app/app.py - Streamlit Dashboard for ML Job Scheduler Simulator

Interactive frontend providing:
- Synthetic workload generation and class distribution visualization
- Modular policy execution with dynamic parameter knobs
- Performance metrics (overall and per-class breakdown)
- Interactive Gantt timeline chart with sliding time window
- Multi-policy comparison benchmark
- Static publication results and figures viewer
"""

import os
import sys
import shutil
import tempfile
import subprocess
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import streamlit as st

# Set page configuration
st.set_page_config(
    page_title="ML Job Scheduler Simulator",
    page_icon="⚡",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom CSS for polished, modern typography and visual aesthetics
st.markdown("""
<style>
    .main-title {
        font-size: 2.2rem;
        font-weight: 700;
        color: #1E293B;
        margin-bottom: 0.2rem;
    }
    .main-subtitle {
        font-size: 1.05rem;
        color: #64748B;
        margin-bottom: 1.5rem;
    }
    .metric-card {
        background-color: #F8FAFC;
        border: 1px solid #E2E8F0;
        border-radius: 8px;
        padding: 16px;
        text-align: center;
    }
    .badge-infer {
        background-color: #DBEAFE;
        color: #1E40AF;
        padding: 4px 8px;
        border-radius: 4px;
        font-weight: 600;
        font-size: 0.85rem;
    }
    .badge-train {
        background-color: #F3E8FF;
        color: #6B21A8;
        padding: 4px 8px;
        border-radius: 4px;
        font-weight: 600;
        font-size: 0.85rem;
    }
    .badge-preproc {
        background-color: #FFEDD5;
        color: #9A3412;
        padding: 4px 8px;
        border-radius: 4px;
        font-weight: 600;
        font-size: 0.85rem;
    }
</style>
""", unsafe_allow_html=True)

# Path resolution relative to repo root
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SIM_EXE = os.path.join(REPO_ROOT, "sim.exe")
DATA_DIR = os.path.join(REPO_ROOT, "data")
DOCS_DIR = os.path.join(REPO_ROOT, "docs")
FIGURES_DIR = os.path.join(DOCS_DIR, "figures")

CLASS_COLORS = {
    "TRAINING": "#9467bd",
    "INFERENCE": "#1f77b4",
    "PREPROCESSING": "#ff7f0e"
}

POLICY_COLORS = {
    "FCFS": "#7f7f7f",
    "SJF": "#bcbd22",
    "SRTF": "#17becf",
    "RR": "#ff7f0e",
    "MLFQ": "#9467bd",
    "EDF": "#2ca02c",
    "Hybrid": "#1f77b4"
}

# Ensure sim.exe exists
if not os.path.exists(SIM_EXE):
    st.error(f"Simulator binary not found at '{SIM_EXE}'. Please run build.bat first.")
    st.stop()

# Helper to run simulator CLI in a temporary directory
def run_simulation(policy: str, workload_path: str, knobs: dict) -> dict:
    temp_dir = tempfile.mkdtemp(prefix="mlsched_run_")
    cmd = [
        SIM_EXE,
        "--policy", policy.lower(),
        "--workload", workload_path,
        "--out", temp_dir
    ]

    pol_lower = policy.lower()
    if pol_lower == "rr":
        if "quantum" in knobs:
            cmd.extend(["--quantum", str(knobs["quantum"])])
    elif pol_lower == "hybrid":
        if "tier1_quantum" in knobs:
            cmd.extend(["--tier1-quantum", str(knobs["tier1_quantum"])])
        if "tier2_quantum" in knobs:
            cmd.extend(["--tier2-quantum", str(knobs["tier2_quantum"])])
        if "aging" in knobs:
            cmd.extend(["--aging", str(knobs["aging"])])
        if "reserve_k" in knobs:
            cmd.extend(["--reserve-k", str(knobs["reserve_k"])])

    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30)
        if proc.returncode != 0:
            shutil.rmtree(temp_dir, ignore_errors=True)
            return {"success": False, "error": proc.stderr or proc.stdout, "returncode": proc.returncode}

        res_file = os.path.join(temp_dir, "results.csv")
        pj_file = os.path.join(temp_dir, "per_job.csv")
        tl_file = os.path.join(temp_dir, "timeline.csv")

        results_df = pd.read_csv(res_file) if os.path.exists(res_file) else None
        per_job_df = pd.read_csv(pj_file) if os.path.exists(pj_file) else None
        timeline_df = pd.read_csv(tl_file) if os.path.exists(tl_file) else None

        shutil.rmtree(temp_dir, ignore_errors=True)
        return {
            "success": True,
            "results": results_df,
            "per_job": per_job_df,
            "timeline": timeline_df
        }
    except Exception as e:
        shutil.rmtree(temp_dir, ignore_errors=True)
        return {"success": False, "error": str(e), "returncode": -1}

# Cache workload generation in session state or temp folder
def generate_workload_file(load: str, seed: int):
    cfg_file = os.path.join(DATA_DIR, f"workload_{load}.cfg")
    if not os.path.exists(cfg_file):
        raise FileNotFoundError(f"Config file not found: {cfg_file}")

    temp_workload_dir = tempfile.mkdtemp(prefix="mlsched_workload_")
    out_csv = os.path.join(temp_workload_dir, f"workload_{load}_{seed}.csv")

    cmd = [
        SIM_EXE,
        "--generate", cfg_file,
        out_csv,
        "--seed", str(seed)
    ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
    if proc.returncode != 0 or not os.path.exists(out_csv):
        shutil.rmtree(temp_workload_dir, ignore_errors=True)
        raise RuntimeError(f"Workload generation failed:\n{proc.stderr}")

    df = pd.read_csv(out_csv)
    return out_csv, temp_workload_dir, df

# ── Sidebar: Inputs & Controls ───────────────────────────────────────────────
with st.sidebar:
    st.markdown("### ⚙️ Workload Configuration")
    load_choice = st.selectbox(
        "Load Level",
        ["light", "medium", "overloaded"],
        index=0,
        help="Select synthetic traffic volume from configuration presets."
    )
    seed_choice = st.number_input(
        "Random Seed",
        min_value=1,
        max_value=999999,
        value=42,
        step=1,
        help="Controls Poisson arrival and burst length RNG reproducibility."
    )

    # Check if workload needs regeneration
    curr_wl_key = f"{load_choice}_{seed_choice}"
    if ("wl_key" not in st.session_state or
        st.session_state["wl_key"] != curr_wl_key or
        "workload_csv" not in st.session_state or
        not os.path.exists(st.session_state["workload_csv"])):

        try:
            wl_path, wl_dir, wl_df = generate_workload_file(load_choice, seed_choice)
            st.session_state["wl_key"] = curr_wl_key
            st.session_state["workload_csv"] = wl_path
            st.session_state["workload_dir"] = wl_dir
            st.session_state["workload_df"] = wl_df
        except Exception as err:
            st.error(f"Failed to generate workload: {err}")
            st.stop()

    wl_df = st.session_state["workload_df"]
    counts = wl_df["class"].str.upper().value_counts()
    n_infer = counts.get("INFERENCE", 0)
    n_train = counts.get("TRAINING", 0)
    n_preproc = counts.get("PREPROCESSING", 0)
    n_total = len(wl_df)

    st.markdown("#### Workload Job Distribution")
    c1, c2 = st.columns(2)
    with c1:
        st.metric("Total Jobs", n_total)
        st.markdown(f"<span class='badge-infer'>Inference: {n_infer}</span>", unsafe_allow_html=True)
    with c2:
        st.markdown(f"<span class='badge-train'>Training: {n_train}</span>", unsafe_allow_html=True)
        st.write("")
        st.markdown(f"<span class='badge-preproc'>Preproc: {n_preproc}</span>", unsafe_allow_html=True)

    st.markdown("---")
    st.markdown("### 🎯 Policy Selection")
    policy_choice = st.selectbox(
        "Scheduling Algorithm",
        ["Hybrid", "FCFS", "SJF", "SRTF", "RR", "MLFQ", "EDF"],
        index=0
    )

    knobs = {}
    if policy_choice == "RR":
        knobs["quantum"] = st.number_input("Round Robin Quantum (q)", min_value=1, max_value=100, value=2, step=1)
    elif policy_choice == "Hybrid":
        st.markdown("**Hybrid Tunable Knobs:**")
        knobs["tier1_quantum"] = st.number_input("Tier 1 Quantum (q1)", min_value=1, max_value=100, value=8, step=1)
        knobs["tier2_quantum"] = st.number_input("Tier 2 Quantum (q2)", min_value=1, max_value=200, value=32, step=2)
        knobs["aging"] = st.number_input("Aging Threshold (ticks)", min_value=0, max_value=10000, value=100, step=10)
        knobs["reserve_k"] = st.number_input("Reserve Period K (every K ticks)", min_value=0, max_value=100, value=10, step=1)

    st.markdown("---")
    run_btn = st.button("🚀 Run Simulation", type="primary", use_container_width=True)

if run_btn:
    with st.spinner(f"Running simulation with {policy_choice}..."):
        res = run_simulation(policy_choice, st.session_state["workload_csv"], knobs)
        if not res["success"]:
            st.error(f"Simulation execution failed (Exit Code {res['returncode']}):\n```\n{res['error']}\n```")
        else:
            st.session_state["last_run_policy"] = policy_choice
            st.session_state["last_run_results"] = res["results"]
            st.session_state["last_run_per_job"] = res["per_job"]
            st.session_state["last_run_timeline"] = res["timeline"]
            st.success(f"Simulation finished successfully for policy '{policy_choice}'.")

# ── Main Header ─────────────────────────────────────────────────────────────
st.markdown("<div class='main-title'>⚡ ML Job Scheduler Simulator Dashboard</div>", unsafe_allow_html=True)
st.markdown("<div class='main-subtitle'>Performance evaluation of CPU/GPU scheduling policies on heterogeneous ML workloads</div>", unsafe_allow_html=True)

# ── Main Tabs ────────────────────────────────────────────────────────────────
tab_metrics, tab_timeline, tab_compare, tab_results = st.tabs([
    "📊 Metrics",
    "⏱️ Timeline (Gantt)",
    "⚔️ Compare Policies",
    "📑 Published Results"
])

# ── TAB 1: Metrics ──────────────────────────────────────────────────────────
with tab_metrics:
    if "last_run_results" not in st.session_state or st.session_state["last_run_results"] is None:
        st.info("👈 Select a policy in the sidebar and click **Run Simulation** to view metrics.")
    else:
        rdf = st.session_state["last_run_results"].iloc[0]
        pol_name = st.session_state["last_run_policy"]

        st.markdown(f"### Performance Metrics: `{pol_name}`")

        # Top KPI Cards
        k1, k2, k3, k4, k5 = st.columns(5)
        with k1:
            st.metric("Makespan", f"{int(rdf['makespan'])} ticks")
        with k2:
            st.metric("Throughput", f"{rdf['throughput']:.4f} jobs/t")
        with k3:
            st.metric("Avg Waiting Time", f"{rdf['avg_wait']:.2f} t")
        with k4:
            st.metric("Inference Miss Rate", f"{rdf['infer_miss_rate']*100:.1f}%")
        with k5:
            st.metric("Starved Jobs", f"{int(rdf['starved_count'])}")

        st.markdown("#### Overall & Per-Class Summary Table")
        table_rows = [
            {
                "Class": "Overall",
                "Job Count": int(rdf["total_jobs"]),
                "Avg Wait (ticks)": f"{rdf['avg_wait']:.2f}",
                "Avg Turnaround (ticks)": f"{rdf['avg_turnaround']:.2f}",
                "Avg Response (ticks)": f"{rdf['avg_response']:.2f}",
                "Deadline Miss Rate": f"{rdf['deadline_miss_rate']*100:.1f}% ({int(rdf['deadline_miss_count'])}/{int(rdf['deadline_count'])})",
                "Max Wait (ticks)": int(rdf["max_wait"]),
                "Starved Jobs": int(rdf["starved_count"])
            },
            {
                "Class": "Inference (Tier 0)",
                "Job Count": int(rdf["infer_jobs"]),
                "Avg Wait (ticks)": f"{rdf['infer_avg_wait']:.2f}",
                "Avg Turnaround (ticks)": f"{rdf['infer_avg_ta']:.2f}",
                "Avg Response (ticks)": f"{rdf['infer_avg_resp']:.2f}",
                "Deadline Miss Rate": f"{rdf['infer_miss_rate']*100:.1f}%",
                "Max Wait (ticks)": int(rdf["infer_max_wait"]),
                "Starved Jobs": int(rdf["infer_starved"])
            },
            {
                "Class": "Preprocessing (Tier 1)",
                "Job Count": int(rdf["preproc_jobs"]),
                "Avg Wait (ticks)": f"{rdf['preproc_avg_wait']:.2f}",
                "Avg Turnaround (ticks)": f"{rdf['preproc_avg_ta']:.2f}",
                "Avg Response (ticks)": f"{rdf['preproc_avg_resp']:.2f}",
                "Deadline Miss Rate": f"{rdf['preproc_miss_rate']*100:.1f}%",
                "Max Wait (ticks)": int(rdf["preproc_max_wait"]),
                "Starved Jobs": int(rdf["preproc_starved"])
            },
            {
                "Class": "Training (Tier 2)",
                "Job Count": int(rdf["train_jobs"]),
                "Avg Wait (ticks)": f"{rdf['train_avg_wait']:.2f}",
                "Avg Turnaround (ticks)": f"{rdf['train_avg_ta']:.2f}",
                "Avg Response (ticks)": f"{rdf['train_avg_resp']:.2f}",
                "Deadline Miss Rate": f"{rdf['train_miss_rate']*100:.1f}%",
                "Max Wait (ticks)": int(rdf["train_max_wait"]),
                "Starved Jobs": int(rdf["train_starved"])
            }
        ]
        st.dataframe(pd.DataFrame(table_rows), use_container_width=True)

        if st.session_state["last_run_per_job"] is not None:
            with st.expander("🔍 View Per-Job Raw Execution Trace"):
                st.dataframe(st.session_state["last_run_per_job"], use_container_width=True)

# ── TAB 2: Timeline (Gantt Chart) ───────────────────────────────────────────
with tab_timeline:
    if "last_run_timeline" not in st.session_state or st.session_state["last_run_timeline"] is None:
        st.info("👈 Run a simulation in the sidebar to visualize the execution timeline.")
    else:
        tdf = st.session_state["last_run_timeline"].copy()
        pol_name = st.session_state["last_run_policy"]

        max_tick = int(tdf["end"].max()) if not tdf.empty else 100
        default_window = min(100, max_tick)

        st.markdown(f"### Gantt Chart: `{pol_name}`")
        st.caption("Inspect discrete execution segments across simulation ticks.")

        w_col1, w_col2 = st.columns([3, 1])
        with w_col1:
            window = st.slider(
                "Time Window Slider (ticks)",
                min_value=0,
                max_value=max(10, max_tick),
                value=(0, default_window),
                step=1
            )
        with w_col2:
            st.metric("Total Makespan", f"{max_tick} ticks")

        w_start, w_end = window
        if w_start >= w_end:
            w_end = w_start + 1

        # Filter segments overlapping with [w_start, w_end]
        visible = tdf[(tdf["end"] > w_start) & (tdf["start"] < w_end)].copy()

        if visible.empty:
            st.warning(f"No execution activity within ticks [{w_start}, {w_end}].")
        else:
            visible["plot_start"] = visible["start"].clip(lower=w_start)
            visible["plot_end"] = visible["end"].clip(upper=w_end)
            visible["duration"] = visible["plot_end"] - visible["plot_start"]

            # Sort job IDs for vertical axis layout
            job_ids = sorted(visible["job_id"].unique())
            fig, ax = plt.subplots(figsize=(14, max(4, len(job_ids) * 0.28)), dpi=180)

            for _, row in visible.iterrows():
                cls_key = str(row["class"]).strip().upper()
                c = CLASS_COLORS.get(cls_key, "#64748B")
                ax.barh(
                    y=row["job_id"],
                    width=row["duration"],
                    left=row["plot_start"],
                    height=0.75,
                    color=c,
                    edgecolor="black",
                    linewidth=0.3
                )

            ax.set_xlim(w_start, w_end)
            ax.set_xlabel("Simulation Tick", fontsize=11, fontweight="bold")
            ax.set_ylabel("Job ID", fontsize=11, fontweight="bold")
            ax.set_title(f"{pol_name} Execution Timeline (Ticks {w_start} to {w_end})", fontsize=12, fontweight="bold", pad=10)
            ax.grid(True, linestyle="--", alpha=0.5, axis="x")

            legend_elements = [
                Patch(facecolor=CLASS_COLORS["INFERENCE"], edgecolor="black", label="Inference (Tier 0 / EDF)"),
                Patch(facecolor=CLASS_COLORS["PREPROCESSING"], edgecolor="black", label="Preprocessing (Tier 1 / FIFO)"),
                Patch(facecolor=CLASS_COLORS["TRAINING"], edgecolor="black", label="Training (Tier 2 / RR)")
            ]
            ax.legend(handles=legend_elements, loc="upper right", framealpha=0.9)
            plt.tight_layout()
            st.pyplot(fig)
            plt.close()

# ── TAB 3: Compare Policies ──────────────────────────────────────────────────
with tab_compare:
    st.markdown("### ⚔️ Multi-Policy Comparative Benchmark")
    st.write(
        f"Run all 7 policies on the active workload (`{load_choice.upper()}`, Seed `{seed_choice}`, `{n_total}` jobs) "
        "to evaluate trade-offs in inference deadline-miss rates, throughput, and starvation."
    )

    comp_btn = st.button("🏁 Run All Policies Comparison", type="secondary")

    if comp_btn or "compare_results" in st.session_state:
        if comp_btn:
            with st.spinner("Executing benchmark across all policies..."):
                policies_to_run = ["FCFS", "SJF", "SRTF", "RR", "MLFQ", "EDF", "Hybrid"]
                comp_rows = []

                progress_bar = st.progress(0)
                for idx, p in enumerate(policies_to_run):
                    res = run_simulation(p, st.session_state["workload_csv"], {})
                    if res["success"] and res["results"] is not None:
                        row = res["results"].iloc[0]
                        comp_rows.append({
                            "Policy": p,
                            "Infer Miss Rate (%)": row["infer_miss_rate"] * 100.0,
                            "Overall Miss Rate (%)": row["deadline_miss_rate"] * 100.0,
                            "Avg Wait (ticks)": row["avg_wait"],
                            "Avg Turnaround (ticks)": row["avg_turnaround"],
                            "Avg Response (ticks)": row["avg_response"],
                            "Throughput (jobs/t)": row["throughput"],
                            "Starved Count": int(row["starved_count"]),
                            "Makespan (ticks)": int(row["makespan"])
                        })
                    progress_bar.progress((idx + 1) / len(policies_to_run))
                progress_bar.empty()
                st.session_state["compare_results"] = pd.DataFrame(comp_rows)

        cdf = st.session_state.get("compare_results")
        if cdf is not None and not cdf.empty:
            st.markdown("#### 1. Inference Deadline-Miss Rate Comparison")
            fig, ax = plt.subplots(figsize=(10, 4.5), dpi=180)
            bars = ax.bar(
                cdf["Policy"],
                cdf["Infer Miss Rate (%)"],
                color=[POLICY_COLORS.get(p, "#333333") for p in cdf["Policy"]],
                edgecolor="black",
                linewidth=0.6,
                width=0.55
            )

            ax.set_ylabel("Inference Miss Rate (%)", fontsize=11, fontweight="bold")
            ax.set_title(f"Inference Deadline-Miss Rate by Policy ({load_choice.capitalize()} Load, Seed {seed_choice})", fontsize=12, fontweight="bold", pad=12)
            ax.set_ylim(0, max(100.0, cdf["Infer Miss Rate (%)"].max() * 1.15))
            ax.grid(True, linestyle="--", alpha=0.5, axis="y")

            for bar in bars:
                height = bar.get_height()
                ax.annotate(f"{height:.1f}%",
                            xy=(bar.get_x() + bar.get_width() / 2, height),
                            xytext=(0, 3),
                            textcoords="offset points",
                            ha="center", va="bottom", fontsize=9, fontweight="bold")

            plt.tight_layout()
            st.pyplot(fig)
            plt.close()

            st.markdown("#### 2. Detailed Performance Comparison Table")
            display_df = cdf.copy()
            format_dict = {
                "Infer Miss Rate (%)": "{:.1f}%",
                "Overall Miss Rate (%)": "{:.1f}%",
                "Avg Wait (ticks)": "{:.2f}",
                "Avg Turnaround (ticks)": "{:.2f}",
                "Avg Response (ticks)": "{:.2f}",
                "Throughput (jobs/t)": "{:.4f}"
            }
            st.dataframe(display_df.style.format(format_dict), use_container_width=True)

# ── TAB 4: Published Results & Figures ──────────────────────────────────────
with tab_results:
    st.markdown("### 📑 Published Benchmark Results & Figures")
    st.write("Aggregated experimental findings across 240 simulation runs (8 policies × 3 load levels × 10 random seeds).")

    summary_csv_path = os.path.join(DOCS_DIR, "results_summary.csv")
    if os.path.exists(summary_csv_path):
        with st.expander("📊 View Aggregated Benchmark Dataset (`docs/results_summary.csv`)", expanded=True):
            s_df = pd.read_csv(summary_csv_path)
            st.dataframe(s_df, use_container_width=True)

    st.markdown("#### Publication Figures (`docs/figures/`)")
    figure_specs = [
        ("fig1_inference_deadline_miss_rate.png", "Figure 1: Inference Deadline-Miss Rate", "Evaluates deadline compliance under Light, Medium, and Overloaded scenarios."),
        ("fig2_waiting_time_by_class.png", "Figure 2: Waiting Time by Workload Class", "Shows how inference, training, and preprocessing jobs wait across policies."),
        ("fig3_training_starvation.png", "Figure 3: Training Job Starvation", "Highlights the anti-starvation aging and reserved capacity benefits."),
        ("fig4_throughput.png", "Figure 4: System Throughput", "Compares total completed jobs per simulation tick across load configurations."),
        ("fig5_gantt_chart.png", "Figure 5: Gantt Execution Comparison", "Trace showing Hybrid preemption vs Round Robin time-slicing."),
        ("scaling.png", "Figure 6: Scaling & Algorithmic Complexity", "Runtime scaling from 1,000 to 100,000 jobs demonstrating O(N log N) scaling.")
    ]

    for i in range(0, len(figure_specs), 2):
        col1, col2 = st.columns(2)
        fname1, title1, cap1 = figure_specs[i]
        path1 = os.path.join(FIGURES_DIR, fname1)
        with col1:
            st.markdown(f"**{title1}**")
            if os.path.exists(path1):
                st.image(path1, caption=cap1, use_container_width=True)
            else:
                st.warning(f"File {fname1} not found in {FIGURES_DIR}")

        if i + 1 < len(figure_specs):
            fname2, title2, cap2 = figure_specs[i + 1]
            path2 = os.path.join(FIGURES_DIR, fname2)
            with col2:
                st.markdown(f"**{title2}**")
                if os.path.exists(path2):
                    st.image(path2, caption=cap2, use_container_width=True)
                else:
                    st.warning(f"File {fname2} not found in {FIGURES_DIR}")
