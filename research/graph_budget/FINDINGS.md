# SKYTRACE 1.9 — measured numerical results

Reuses frozen SKYTRACE 1.8 direct-position synthetic trajectories (220 train / 70 validation / 70 held out). See `PREREGISTRATION.md` for controls and limitations. Units for error: rad/s. Timings: this shared container, not hardware-normalized.

| Branch | Held-out MAE | p95 representation+prediction (ms) | 50 ms p95 gate |
|---|---:|---:|---|
| full_attributed_graph | 0.078241 | 0.091 | pass |
| temporal | 0.000733 | 0.382 | pass |
| braid_word | 0.026321 | 0.175 | pass |
| braid_timing | 0.024794 | 0.437 | pass |
| braid_metrics | 0.000746 | 0.639 | pass |
| Direct physical model fit | 0.000542 | mean 2.129 | not evaluated |

**Interpretation:** The full attributed graph's linear ridge estimator performs poorly because it does not learn the translation/rotation/heading invariants intrinsic to constant-turn motion. This does **not** show the complete graph is uninformative. Temporal feature engineering does much better; braid+metric closely matches temporal because it contains temporal features. Braid-only remains non-identifying for the registered collision control. All 5 learned branches passed the illustrative 50 ms p95 threshold, which therefore cannot establish relative budget efficiency. A constrained equal-total-compute claim needs pinned, isolated hardware and an estimator family that can exploit full graph geometry.

The data-generation and benchmark software were run locally, with six new unit tests passing. No real PURSUE/GIMBAL sensor records were used.
