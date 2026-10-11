# Paired Comparison: Inference Deadline-Miss Rate (Hybrid vs. Other Policies)

This analysis evaluates the paired differences in inference deadline-miss rate between **Hybrid** and each alternative policy evaluated on the **exact same 10 random seeds** (seeds 1 to 10).

- **Metric:** `infer_miss_rate` from `results/runs_raw.csv`
- **Paired Difference:** $\Delta = \text{MissRate}_{\text{Other}} - \text{MissRate}_{\text{Hybrid}}$ (positive $\Delta$ indicates Hybrid achieved a lower miss rate)
- **Seeds Won (Hybrid Better):** Count of seeds (out of 10) where $\text{MissRate}_{\text{Hybrid}} < \text{MissRate}_{\text{Other}}$
- **Seeds Tied:** Count of seeds where miss rates are equal
- **Seeds Lost (Other Better):** Count of seeds where $\text{MissRate}_{\text{Other}} < \text{MissRate}_{\text{Hybrid}}$
- **Statistical Significance Criterion:** When Hybrid wins at least 4 out of 10 seeds (or ties), differences against policies claiming a win are **not distinguishable from noise**.

## Load: Light

| Policy Comparison | Mean Other | Mean Hybrid | Mean Paired Diff (Other - Hybrid) | Seeds Won (Hybrid Better) | Seeds Tied | Seeds Lost (Other Better) | Assessment |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| Hybrid vs. FCFS | 0.2752 | 0.0225 | +0.2527 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SJF | 0.2343 | 0.0225 | +0.2119 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SRTF | 0.0317 | 0.0225 | +0.0093 | **6/10** | 3/10 | 1/10 | **Hybrid clearly superior** |
| Hybrid vs. RR-small | 0.2335 | 0.0225 | +0.2110 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. RR-large | 0.2636 | 0.0225 | +0.2411 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. MLFQ | 0.0989 | 0.0225 | +0.0764 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. EDF | 0.0178 | 0.0225 | -0.0047 | **1/10** | 6/10 | 3/10 | **EDF favored** |

## Load: Medium

| Policy Comparison | Mean Other | Mean Hybrid | Mean Paired Diff (Other - Hybrid) | Seeds Won (Hybrid Better) | Seeds Tied | Seeds Lost (Other Better) | Assessment |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| Hybrid vs. FCFS | 0.7580 | 0.1141 | +0.6438 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SJF | 0.5993 | 0.1141 | +0.4851 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SRTF | 0.1091 | 0.1141 | -0.0051 | **3/10** | 1/10 | 6/10 | **SRTF statistically superior** |
| Hybrid vs. RR-small | 0.7199 | 0.1141 | +0.6058 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. RR-large | 0.7486 | 0.1141 | +0.6344 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. MLFQ | 0.2761 | 0.1141 | +0.1620 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. EDF | 0.2575 | 0.1141 | +0.1434 | **9/10** | 0/10 | 1/10 | **Hybrid clearly superior** |

## Load: Overloaded

| Policy Comparison | Mean Other | Mean Hybrid | Mean Paired Diff (Other - Hybrid) | Seeds Won (Hybrid Better) | Seeds Tied | Seeds Lost (Other Better) | Assessment |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| Hybrid vs. FCFS | 0.9648 | 0.4598 | +0.5050 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SJF | 0.7181 | 0.4598 | +0.2583 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. SRTF | 0.3505 | 0.4598 | -0.1093 | **0/10** | 0/10 | 10/10 | **SRTF statistically superior** |
| Hybrid vs. RR-small | 0.9689 | 0.4598 | +0.5091 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. RR-large | 0.9638 | 0.4598 | +0.5040 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. MLFQ | 0.7270 | 0.4598 | +0.2673 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |
| Hybrid vs. EDF | 0.8995 | 0.4598 | +0.4397 | **10/10** | 0/10 | 0/10 | **Hybrid clearly superior** |

---

## Detailed Per-Seed Data

### Per-Seed Inference Deadline-Miss Rates: Light

| Seed | Hybrid | FCFS | SJF | SRTF | RR-small | RR-large | MLFQ | EDF |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 0.0460 | 0.1954 | 0.1609 | 0.0575 | 0.1954 | 0.1954 | 0.1494 | 0.0460 |
| 2 | 0.0135 | 0.1216 | 0.0946 | 0.0135 | 0.0946 | 0.1081 | 0.0676 | 0.0135 |
| 3 | 0.0000 | 0.1081 | 0.0946 | 0.0000 | 0.0811 | 0.0946 | 0.0135 | 0.0000 |
| 4 | 0.0429 | 0.2143 | 0.2000 | 0.0429 | 0.1857 | 0.2143 | 0.1000 | 0.0286 |
| 5 | 0.0690 | 0.3966 | 0.3621 | 0.0517 | 0.3966 | 0.3793 | 0.1552 | 0.0345 |
| 6 | 0.0000 | 0.1884 | 0.1594 | 0.0145 | 0.1739 | 0.1739 | 0.1014 | 0.0000 |
| 7 | 0.0000 | 0.2656 | 0.2031 | 0.0156 | 0.1250 | 0.2500 | 0.0469 | 0.0000 |
| 8 | 0.0000 | 0.3889 | 0.3333 | 0.0278 | 0.3194 | 0.3750 | 0.0972 | 0.0278 |
| 9 | 0.0145 | 0.2754 | 0.2029 | 0.0290 | 0.2174 | 0.2609 | 0.1014 | 0.0145 |
| 10 | 0.0390 | 0.5974 | 0.5325 | 0.0649 | 0.5455 | 0.5844 | 0.1558 | 0.0130 |

### Per-Seed Inference Deadline-Miss Rates: Medium

| Seed | Hybrid | FCFS | SJF | SRTF | RR-small | RR-large | MLFQ | EDF |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 0.2000 | 0.7448 | 0.6207 | 0.1862 | 0.7310 | 0.7448 | 0.3862 | 0.3034 |
| 2 | 0.1496 | 0.8740 | 0.6850 | 0.0866 | 0.8268 | 0.8740 | 0.3465 | 0.2205 |
| 3 | 0.0750 | 0.5250 | 0.4500 | 0.0833 | 0.4417 | 0.4917 | 0.1750 | 0.1167 |
| 4 | 0.1121 | 0.7931 | 0.6293 | 0.1034 | 0.7931 | 0.7845 | 0.3707 | 0.3534 |
| 5 | 0.1200 | 0.9600 | 0.7300 | 0.1800 | 0.9100 | 0.9600 | 0.2900 | 0.3000 |
| 6 | 0.1186 | 0.9407 | 0.8051 | 0.1102 | 0.9068 | 0.9322 | 0.2627 | 0.2627 |
| 7 | 0.0763 | 0.4237 | 0.3644 | 0.0763 | 0.3136 | 0.4068 | 0.1271 | 0.0847 |
| 8 | 0.0513 | 0.4786 | 0.3333 | 0.0342 | 0.4274 | 0.4615 | 0.1111 | 0.0342 |
| 9 | 0.1019 | 0.9259 | 0.7130 | 0.0648 | 0.9352 | 0.9167 | 0.2315 | 0.2593 |
| 10 | 0.1367 | 0.9137 | 0.6619 | 0.1655 | 0.9137 | 0.9137 | 0.4604 | 0.6403 |

### Per-Seed Inference Deadline-Miss Rates: Overloaded

| Seed | Hybrid | FCFS | SJF | SRTF | RR-small | RR-large | MLFQ | EDF |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 0.6390 | 0.9710 | 0.7137 | 0.4357 | 0.9751 | 0.9710 | 0.7884 | 0.9461 |
| 2 | 0.4375 | 1.0000 | 0.7212 | 0.3510 | 1.0000 | 1.0000 | 0.7452 | 0.9856 |
| 3 | 0.4950 | 0.9950 | 0.8150 | 0.3800 | 0.9950 | 0.9950 | 0.7850 | 0.9800 |
| 4 | 0.4611 | 0.9223 | 0.7202 | 0.4249 | 0.9378 | 0.9223 | 0.7824 | 0.9119 |
| 5 | 0.3043 | 0.9441 | 0.6460 | 0.2733 | 0.9503 | 0.9441 | 0.7081 | 0.9130 |
| 6 | 0.4523 | 0.9548 | 0.7337 | 0.2814 | 0.9447 | 0.9548 | 0.6080 | 0.6482 |
| 7 | 0.4208 | 0.9950 | 0.6881 | 0.3416 | 1.0000 | 0.9950 | 0.6634 | 0.9158 |
| 8 | 0.4000 | 0.9641 | 0.7026 | 0.2615 | 0.9744 | 0.9641 | 0.7436 | 0.9436 |
| 9 | 0.4697 | 0.9293 | 0.7273 | 0.3788 | 0.9343 | 0.9192 | 0.6465 | 0.7828 |
| 10 | 0.5182 | 0.9727 | 0.7136 | 0.3773 | 0.9773 | 0.9727 | 0.8000 | 0.9682 |

