# SKYTRACE 1.9 — attributed graph and measured-budget follow-up

## Scope
This follow-up reuses **exactly** the 1.8 constant-turn, direct-position synthetic dataset, RNG seed 20261117, trajectory splits (220/70/70), Gaussian sensor noise, and collision control. It is not a study of real UAP telemetry.

## Primary target
Angular turn rate `omega` (rad/s). All branches share target and the same train/validation/test groups. Reports keep MAE, RMSE, bias, split-residual 90% marginal coverage and interval width separate from computation costs.

## New baseline
`full_attributed_graph`: the 18 timestamped measurement vertices, position values relative to the first observation, first-vertex calibration sigma, and implicit directed temporal edges `i -> i+1`. Node and edge order follows timestamps. A ridge regressor with the exact 1.8 penalty grid is fitted to these uncompressed per-vertex attributes. Unlike 1.8's lossy 10-feature `graph_summary`, this representation retains all 18 measured positions (up to translation), all times, and calibration sigma. **This does not mean the ridge model is a universal graph estimator:** it has no engineered orientation invariance or nonlinear dynamical inductive bias. Nonlinear/GNN baselines would be another controlled study.

## Cost policy
Compute cost is observed on this shared host, not hardware-pinned. The per-track inference gate is a p95 **50 ms** budget for representation construction plus prediction. Training+tuning and all-split representation construction are reported separately. All candidate models receive the same fixed alpha search grid. No claim of matching total computational budgets or statistically stable latency across machines is warranted. For genuine total compute parity, run a controlled fixed-core benchmarking campaign with caps on feature construction, hyperparameter optimization and inference, repeated cold and warm runs and resource isolation.

## Regression controls
- Fixed 0.04-vs-0.12 rad/s positive-turn collision of braid words must persist.
- Exact duplicate record must not change any feature vector.
- Changing a sample must alter the full attributed graph vector.
- Invalid timestamp ordering must be rejected.
- All measured features must be finite.

## Extension gate: range–bearing
Before range–bearing model work, register sensor position/orientation, clocks and offset priors, measurement covariance, range-missingness mechanisms, and identifiability assumptions. Comparisons must preserve the same underlying trajectories across modalities and origin identities. Outcomes to preregister: turn-rate error, failure under unidentifiable geometry, marginal interval coverage, synchronization sensitivity, duplicate invariance, cost.
