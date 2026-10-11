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

Hybrid is optimized to protect inference deadlines while reserving capacity for training progress. Consequently, other policies outperform Hybrid on several metrics where different tradeoffs are favored. Seed-paired comparisons across all 10 evaluation seeds (detailed in [docs/PAIRED.md](PAIRED.md)) quantify the statistical robustness of each claimed tradeoff:

### Light Load
- **Inference Deadline-Miss Rate:** Pure **EDF** achieves $0.0178 \pm 0.0160$ vs. Hybrid's $0.0225 \pm 0.0249$ (Hybrid wins 1/10 seeds, ties 6/10 seeds, EDF wins 3/10 seeds). Because Hybrid guarantees 1 out of every 10 ticks ($K=10$) to background training, an inference job with an imminent deadline can occasionally miss when displaced by a reserved tick.
- **Inference Waiting Time:**
  - vs. **SRTF** ($0.1162$ vs. Hybrid $0.2079$): Hybrid wins 5 of 10 seeds and ties 1 seed (**not distinguishable from noise**; Hybrid wins $\ge 4/10$ seeds).
  - vs. **EDF** ($0.1504$ vs. Hybrid $0.2079$): Hybrid wins 1 of 10 seeds, ties 1 seed (EDF wins 8 of 10 seeds).
  - vs. **SJF** ($3.1601$ vs. Hybrid $0.2079$): Hybrid wins 10 of 10 seeds (**not distinguishable from noise / claim invalid**; non-preemptive SJF delays inference behind long training bursts).
- **Response Time:** **MLFQ** achieves lower overall average response time ($0.3804$ vs. Hybrid's $1.2678$, Hybrid wins 0/10 seeds) and preprocessing response time ($0.8872$ vs. Hybrid's $3.5786$, Hybrid wins 0/10 seeds) because all arriving jobs enter the top priority queue upon admission.
- **Training Waiting Time:** **FCFS** ($20.94$, Hybrid wins 0/10 seeds, ties 1/10) and **SJF** ($28.31$, Hybrid wins 0/10 seeds, ties 1/10) achieve lower average training waiting time than Hybrid ($44.64$).

### Medium Load
- **Inference Deadline-Miss Rate & Waiting Time:** **SRTF** achieves slightly lower inference deadline miss rate ($0.1091$ vs. Hybrid's $0.1141$, Hybrid wins 3/10 seeds, ties 1/10 seeds) and inference waiting time ($0.6178$ vs. Hybrid's $0.7234$, Hybrid wins 2/10 seeds, ties 1/10 seeds) by greedily prioritizing remaining burst length over arrival order.
- **Overall Waiting & Turnaround Time:** **SRTF** (avg wait $13.25$, turnaround $19.14$, Hybrid wins 0/10 seeds) and **EDF** (avg wait $17.50$, turnaround $23.39$, Hybrid wins 1/10 seeds) outperform Hybrid (avg wait $21.78$, turnaround $27.67$).
- **Training Degradation:** Because inference jobs in Tier 0 take precedence, training jobs experience higher waiting times under Hybrid ($229.66$) than under **FCFS** ($58.03$, Hybrid wins 0/10 seeds), **SJF** ($127.18$, Hybrid wins 0/10 seeds), **SRTF** ($173.30$, Hybrid wins 0/10 seeds), and **EDF** ($182.57$, Hybrid wins 0/10 seeds, ties 1/10).
- **Preprocessing Deadlines:** Preprocessing deadline miss rate is higher under Hybrid ($0.8297$) than under **SRTF** ($0.4792$, Hybrid wins 0/10 seeds), **EDF** ($0.5266$, Hybrid wins 0/10 seeds), and **SJF** ($0.6665$, Hybrid wins 0/10 seeds, ties 1/10).

### Overloaded Load
- **Inference Deadline-Miss Rate:** **SRTF** achieves $0.3505 \pm 0.0768$ miss rate compared to Hybrid's $0.4598 \pm 0.0861$ (Hybrid wins 0/10 seeds, SRTF wins 10/10 seeds).
- **Overall Average Waiting Time:** **SRTF** ($82.08$, Hybrid wins 0/10 seeds) and **SJF** ($87.28$, Hybrid wins 0/10 seeds) achieve substantially lower overall waiting times than Hybrid ($144.17$).
- **Training Waiting Time:** **FCFS** ($410.94$, Hybrid wins 0/10 seeds), **SJF** ($928.86$, Hybrid wins 0/10 seeds), **SRTF** ($963.51$, Hybrid wins 0/10 seeds), and **EDF** ($986.66$, Hybrid wins 0/10 seeds) all achieve lower training waiting times than Hybrid ($1145.71$).
- **Preprocessing Starvation:** Under severe overload, preprocessing jobs that demote to Tier 2 experience high wait times ($537.46$ under Hybrid vs. $159.90$ under EDF [Hybrid wins 0/10 seeds] and $261.13$ under SRTF [Hybrid wins 0/10 seeds]) and a $1.0000$ deadline miss rate.
- **Maximum Waiting Time (Starvation):** **FCFS** bounds max wait time across all jobs to $856.10$ ticks (Hybrid wins 0/10 seeds), whereas Hybrid reaches $1450.50$ ticks for long training jobs delayed by inference bursts.

---

## 4. Data-Backed Analysis: Why EDF Misses ~26% of Inference Deadlines at Medium Load

At medium load, total workload demand is approximately 80.3% of simulation capacity (e.g. seed 1 generates 57,573 demand ticks across a 67,114-tick horizon), yet pure EDF suffers an average inference deadline-miss rate of **$0.2575 \pm 0.1702$ (25.8%)** alongside a **$0.5135 \pm 0.2079$ (51.4%)** preprocessing miss rate. Simulator outputs and workload configuration reveal two compounding mechanisms:

1. **Tight Inference Slack vs. Preprocessing Burst Arrival:** In `data/workload_medium.cfg`, inference requests have a tight slack factor of $1.5$ ($\text{deadline} = \text{arrival} + \lceil\text{burst} \times 1.5\rceil$), allowing only 1 to 2 ticks of queuing delay for burst 1–3 requests. Preprocessing jobs arrive in simultaneous batches of 3 (`preproc_batch_size = 3`) with bursts of 8–12 ticks (mean 10 ticks, totaling ~30 contiguous ticks of demand per batch) and slack factor $3.0$ ($\text{deadline} = \text{arrival} + 30$).
2. **EDF Domino Collapses from Expired Deadlines:** Pure EDF places all deadline-bearing jobs into a single min-heap and does **not drop overdue jobs**. When a batch of preprocessing jobs is scheduled, their earlier absolute deadlines grant them execution priority. Once transient queuing causes a preprocessing job to miss its deadline, its expired deadline remains small, creating head-of-line blocking for subsequent arrivals. For example, in medium load seed 1 at $t=591$, preprocessing batch jobs 109, 110, and 111 arrive with deadlines 618–624. They execute back-to-back until $t=652$, finishing 13, 22, and 28 ticks late. During this entire 61-tick window, their expired deadlines strictly precede those of newly arriving inference jobs (e.g., Job 115 arrives at $t=617$ with deadline 619, Job 116 at $t=633$ with deadline 638, Jobs 117–121 arriving $t=638\dots654$). All seven inference jobs are queued behind the overdue preprocessing work and miss their deadlines by 12 to 22 ticks. Even on smaller Poisson clusters (e.g. $t=114\dots120$, where Job 17 with burst 4 delays Job 18 with burst 3 to finish at $t=121 > 120$), slack 1.5 provides zero queue buffer. In contrast, Hybrid isolates inference into Tier 0, ensuring that preprocessing never preempts or delays inference requests.

---

## 5. Figures

The generated figures are available in `docs/figures/`:
1. `fig1_inference_deadline_miss_rate.png`: Inference deadline-miss rate vs. workload load.
2. `fig2_waiting_time_by_class.png`: Average waiting time per job class for each load.
3. `fig3_training_starvation.png`: Max waiting time of training jobs across policies.
4. `fig4_throughput.png`: System throughput per policy and load.
5. `fig5_gantt_chart.png`: Timeline execution Gantt chart (Hybrid vs. RR-small) on a light workload sample.
