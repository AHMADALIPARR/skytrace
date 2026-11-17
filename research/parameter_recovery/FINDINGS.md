# Findings — SKYTRACE 1.8

The measurements below are from a reproducible **synthetic** fixed-seed experiment, with 70 held-out trajectories. The bench uses direct position observations; no ranges, bearings, classified feeds, or UAP videos were used.

| Branch | Test MAE (rad/s) | 90% interval coverage | Full width (rad/s) |
|---|---:|---:|---:|
| Lossy graph summary | 0.07824 | 97.1% | 0.3195 |
| Temporal | 0.00073 | 85.7% | 0.0031 |
| Braid word | 0.02632 | 88.6% | 0.1102 |
| Braid with timing | 0.02479 | 95.7% | 0.1202 |
| Braid with metric attributes | 0.00075 | 90.0% | 0.0032 |
| Direct model-fitting reference | 0.00054 | not computed | not computed |

Values are rounded from `results.synthetic.json`. Timings depend on the machine and should be regenerated locally rather than interpreted as portable resource comparisons. The braid-plus-metric branch contains the temporal features and should not be interpreted as independent improvement from topology.

**Identifiability control:** the deterministic braid word is identical for trajectories with turn rates 0.04 and 0.12 rad/s (same sampling cadence, no measurement noise), while the temporal features distinguish them. A braid-only estimator cannot output separate values for these two identical encodings.

**Exact-record provenance duplication:** canonicalization preserves predictions for all five branches when an identical recording is repeated. Different sensor measurements remain distinct records even if they share a source origin.

## Frozen-model robustness (MAE in rad/s)

| Intervention | Temporal | Braid word | Braid + timing | Braid + metrics |
|---|---:|---:|---:|---:|
| Base | 0.0007 | 0.0263 | 0.0248 | 0.0007 |
| Add 0.1 m Gaussian noise | 0.0020 | 0.0462 | 0.0451 | 0.0020 |
| Jitter timestamps by 0.03 s | 0.0007 | 0.0263 | 0.0248 | 0.0007 |
| Drop every fourth measurement | 0.0007 | 0.0299 | 0.0294 | 0.0007 |
| Halve observation duration | 0.0033 | 0.0381 | 0.0284 | 0.0033 |

Full precision is retained in `robustness.synthetic.json`.

**Conclusion:** standard temporal features recover turn rate much better than the braid-only encoding in this setting. The braid augmentation does not materially improve parameter recovery. Real next milestones: calibrated range-bearing models, physical sensor synchronization, full graph estimator, proper matched compute caps, coverage assessment across repeated trials, and formal proof of the duplicate-canonicalization equivalence relation.
