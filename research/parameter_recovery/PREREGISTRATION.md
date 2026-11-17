# SKYTRACE 1.8 — Constant-turn parameter recovery

Status: synthetic baseline evaluation. Not a claim about UAP physics, quantum effects, or braid superiority.

## Target, generative model, and units

The ground-truth target is constant angular turn rate `omega` (rad/s). The exact continuous solution for position is used with speed in m/s, time in s, and coordinates in a local Cartesian plane (m). The 360 generated trajectories independently vary angular velocity, speed, heading, initial position, cadence, and positional noise. Each has 18 nominal samples. Positions are directly measured with Gaussian 0.035 m per-axis noise. No actual sensor records are incorporated. Source and origin metadata are preserved in exact-record canonicalization.

## Frozen splits

NumPy seed 20261117, trajectory-level split of 220 training, 70 validation, 70 held-out test cases. No duplicated measurement from an underlying trajectory crosses split boundaries. Validation chooses ridge penalty from {0.01, 0.1, 1, 10, 100}; a 90%-nominal interval uses the upper 90th percentile of absolute validation residuals. This simple split-residual interval has no conditional-coverage guarantee.

## Branches

* **Graph summary:** fixed vector of attributed graph/track statistics; intentionally compresses temporal order and serves as a lossy graph-summary baseline, not full-graph optimal inference.
* **Temporal:** unwrapped heading versus timestamp, turn-rate, displacement, time and cadence features.
* **Braid word:** predeclared crossing signs from consecutive local heading changes, alternating B3 generators. It deliberately omits physical magnitudes; it is a modeling encoding, not proof of actual braiding.
* **Braid + timing:** word features plus crossing interval information.
* **Braid + metric:** the braid features concatenated with the temporal features; it can never be claimed as a braid-only improvement.
* **Direct model fitting:** nonlinear least squares under the exact constant-turn position model; serves as a physical reference.

Ridge regression and the same penalty grid are used for all learned branches. No graph neural network or full attributed-graph estimator has been implemented; the current graph baseline is a lossy summary.

## Primary evaluation

Report held-out MAE/RMSE/bias in rad/s, held-out coverage and width of nominal 90% intervals, exact-duplicate invariance, word-only collisions, times for feature construction, validation tuning and inference. Local wall clock is indicative only, not an enforcement of a fixed equal compute budget. Include direct model fitting latency separately. Model-fitting reference has no uncertainty interval.

## Robustness

Fit all learned branches once on the base training set; evaluate the same held-out underlying trajectories with additional 0.1 m position noise, independent 0.03 s timestamp perturbation, every-fourth-sample removal, and half observation duration. Do not retrain on corrupted cases. The robustness script adds the perturbation after generating ground truth, preserving targets and split membership.

## Acceptance and interpretation

The proposed braid advantage requires a lower error than temporal features under a genuinely equal resource cap and no material coverage degradation. No such superiority is claimed here. A braid word collision between omega=0.04 and 0.12 rad/s demonstrates why discrete sign data are insufficient to uniquely recover turn magnitude. A true fair budget comparison also requires repeated hardware-pinned benchmarking and a full attributed-graph estimator; both are follow-on requirements.

Run:

```bash
python3 -m unittest discover -s research/parameter_recovery/tests -v
python3 research/parameter_recovery/benchmark.py
python3 -c 'from research.parameter_recovery.benchmark import robustness; import json; print(json.dumps(robustness(),indent=2))'
```
