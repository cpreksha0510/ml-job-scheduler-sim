# Experimental Results: Comparative Scheduler Evaluation

Benchmark results comparing 8 scheduling policies across three load configurations (**light**, **medium**, **overloaded**) evaluated over 10 random seeds (seeds 1 to 10). Workloads model mixed ML infrastructure consisting of latency-sensitive **inference** requests with deadlines, throughput-bound **training** jobs, and batch **preprocessing** jobs.

---

## 1. Inference Deadline-Miss Rate Comparison

Mean and standard deviation of deadline-miss rate for inference requests across 10 evaluation seeds:

| Workload Load | Hybrid | RR-small ($q=2$) | RR-large ($q=8$) | EDF | MLFQ |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Light** | $0.0225 \pm 0.0249$ | $0.2335 \pm 0.1465$ | $0.2636 \pm 0.1479$ | $0.0178 \pm 0.0160$ | $0.0989 \pm 0.0471$ |
| **Medium** | $0.1141 \pm 0.0425$ | $0.7199 \pm 0.2357$ | $0.7486 \pm 0.2150$ | $0.2575 \pm 0.1702$ | $0.2761 \pm 0.1164$ |
| **Overloaded** | $0.4598 \pm 0.0861$ | $0.9689 \pm 0.0255$ | $0.9638 \pm 0.0290$ | $0.8995 \pm 0.1054$ | $0.7270 \pm 0.0676$ |

---

## 2. Reduction in Inference Deadline Misses

Percent reduction achieved by Hybrid relative to Round Robin variants, computed as:
$$\text{Reduction} = \frac{\text{Miss Rate}_{\text{RR}} - \text{Miss Rate}_{\text{Hybrid}}}{\text{Miss Rate}_{\text{RR}}} \times 100\%$$

| Workload Load | vs. RR-small ($q=2$) | vs. RR-large ($q=8$) |
|:---|:---:|:---:|
| **Light** | **90.37%** ($0.2335 \to 0.0225$) | **91.47%** ($0.2636 \to 0.0225$) |
| **Medium** | **84.15%** ($0.7199 \to 0.1141$) | **84.75%** ($0.7486 \to 0.1141$) |
| **Overloaded** | **52.54%** ($0.9689 \to 0.4598$) | **52.29%** ($0.9638 \to 0.4598$) |

---

## 3. Where Hybrid Does Not Win

Hybrid is optimized to protect inference deadlines while reserving capacity for training progress. Consequently, other policies outperform Hybrid on several metrics where different tradeoffs are favored:

### Light Load
- **Inference Deadline-Miss Rate:** Pure **EDF** achieves $0.0178 \pm 0.0160$ vs. Hybrid's $0.0225 \pm 0.0249$. Because Hybrid guarantees 1 out of every 10 ticks ($K=10$) to background training, an inference job with an imminent deadline can occasionally miss when displaced by a reserved tick.
- **Inference Waiting Time:** **SRTF** ($0.1162$), **EDF** ($0.1504$), and **SJF** ($0.1517$) achieve lower inference waiting time than Hybrid ($0.2079$).
- **Response Time:** **MLFQ** achieves the lowest overall average response time ($0.3804$ vs. Hybrid's $1.2678$) and preprocessing response time ($0.8872$ vs. Hybrid's $3.5786$) because all arriving jobs enter the top priority queue upon admission.
- **Training Waiting Time:** **FCFS** ($20.94$) and **SJF** ($28.31$) achieve lower average training waiting time than Hybrid ($44.64$).

### Medium Load
- **Inference Deadline-Miss Rate & Waiting Time:** **SRTF** achieves slightly lower inference deadline miss rate ($0.1091$ vs. Hybrid's $0.1141$) and inference waiting time ($0.6178$ vs. Hybrid's $0.7234$) by greedily prioritizing remaining burst length over arrival order.
- **Overall Waiting & Turnaround Time:** **SRTF** (avg wait $13.25$, turnaround $19.14$) and **EDF** (avg wait $17.50$, turnaround $23.39$) outperform Hybrid (avg wait $21.78$, turnaround $27.67$).
- **Training Degradation:** Because inference jobs in Tier 0 take precedence, training jobs experience higher waiting times under Hybrid ($229.66$) than under **FCFS** ($58.03$), **SJF** ($127.18$), **SRTF** ($173.30$), and **EDF** ($182.57$).
- **Preprocessing Deadlines:** Preprocessing deadline miss rate is higher under Hybrid ($0.8297$) than under **SRTF** ($0.4792$), **EDF** ($0.5266$), and **SJF** ($0.6665$).

### Overloaded Load
- **Inference Deadline-Miss Rate:** **SRTF** achieves $0.3505 \pm 0.0768$ miss rate compared to Hybrid's $0.4598 \pm 0.0861$.
- **Overall Average Waiting Time:** **SRTF** ($82.08$) and **SJF** ($87.28$) achieve substantially lower overall waiting times than Hybrid ($144.17$).
- **Training Waiting Time:** **FCFS** ($410.94$), **SJF** ($928.86$), **SRTF** ($963.51$), and **EDF** ($986.66$) all achieve lower training waiting times than Hybrid ($1145.71$).
- **Preprocessing Starvation:** Under severe overload, preprocessing jobs that demote to Tier 2 experience high wait times ($537.46$ under Hybrid vs. $159.90$ under EDF and $261.13$ under SRTF) and a $1.0000$ deadline miss rate.
- **Maximum Waiting Time (Starvation):** **FCFS** bounds max wait time across all jobs to $856.10$ ticks, whereas Hybrid reaches $1450.50$ ticks for long training jobs delayed by inference bursts.

---

## 4. Figures

The generated figures are available in `docs/figures/`:
1. `fig1_inference_deadline_miss_rate.png`: Inference deadline-miss rate vs. workload load.
2. `fig2_waiting_time_by_class.png`: Average waiting time per job class for each load.
3. `fig3_training_starvation.png`: Max waiting time of training jobs across policies.
4. `fig4_throughput.png`: System throughput per policy and load.
5. `fig5_gantt_chart.png`: Timeline execution Gantt chart (Hybrid vs. RR-small) on a light workload sample.
