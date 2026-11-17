# SKYTRACE 2.2 Numerical Release

Organized prose from `source3.md`. This source contained no fenced or unfenced source-code chunks. Wording is unchanged; only markdown structure, list formatting, and trailing whitespace were cleaned up.

## SKYTRACE 2.2: Independent scientific release gates

## Scope and scientific interpretation

**All physical targets are synthetic. No UAP telemetry has been analyzed by this release. benchmark.py preserves the original SKYTRACE 2.1 generator and source file, and its original reference results, without changing either. The 2.1 repeat benchmark’s archived values are compared with tolerance 1e-10 rad/s.**

## G0. Regression

- Fixed seed 20261117, n=180, split 100 training / 40 validation / 40 testing, 2.1 generator intact.
- All four 2.1 MAEs reproduce within 1e-10 rad/s in the tested environment.
- The original reference is research/joint_fusion/results.synthetic.json; its source SHA-256 is included in results.synthetic.json.

## G1. Repeated-seed uncertainty

- Five synthetic datasets with seeds 20261117–20261121; each has 40 held-out trajectories. Mean seed MAE, sample SD, descriptive t-based 95% CI of mean MAE, nominal 90% coverage, and mean interval width are reported independently for each branch.
- The 95% intervals reflect variability across the five seed-level aggregate MAEs, not measurement error or independent posterior inference. Five seeds are insufficient for a definitive calibration claim.
- The legacy 2.1 validation-residual quantile method is retained for apples-to-apples comparison. The result is not an exact finite-sample 90% conformal coverage guarantee under changed data distributions.

## G2. Physical clock inference

- Three physically fixed sensors: (-22,-12), (24,-9), (-7,27) m.
- Constant turn target: [x0,y0,v,heading,omega] in SI units; omega in rad/s.
- Twenty observations per sensor over six seconds, with independent 0.003 rad bearing and 0.10 m range Gaussian errors.
- Sensor 0 is the reference clock, delta_0=0; unknown sensors 1 and 2 have injected offsets uniformly within ±0.18 seconds. Physical target positions are computed at time t, while sensor-reported times are t+delta_i.
- Fit with weighted nonlinear least squares, comparing known-offset baseline (reference) and joint seven-parameter model (unknown offsets).
- Report absolute omega error, trajectory RMSE, clock offset MAE, solver iterations, and convergence flag across 12 independently seeded cases.
- Do not confuse clock-error recovery with the legacy 2.1 timestamp generation, which shifts physical and recorded times together.
- Convergence criterion for this fixture: solver reports success and normalized residual norm below 250. This is a benchmark check, not a universal goodness-of-fit test.

## G3. Protocol integration

- kernel_port.py supports NDJSON, one JSON response for each input line; caps individual input lines at 65,536 bytes; rejects malformed operations without losing the next request. Full Python process protocol has a passing test with valid → invalid → valid sequence.
- beam/lib/skytrace/release22.ex provides bounded Task.Supervisor submission. Elixir/OTP is not installed in this environment, so actual BEAM process throughput, RSS, queue sizes, backpressure, restart intensity and recovery remain unverified. These are required for full integrated release, and not replaced by the Python port test.
- The Python peak_process_rss_kib in the results is a high-water mark for this benchmark process, not a measure of BEAM integration. Do not compare it to the Python allocation peak from 2.1.

## G4. Fibonacci ablation

- Same fused bearing/range temporal features plus eight real/imaginary components of a two-dimensional Fibonacci braid matrix; same training/validation/test trajectories, same ridge alpha candidates and model family.
- Braid generators satisfy the B3 relation and numerical unitarity. Braid features are functions of the same fused angular-rate history and add no raw measurements.
- Report MAE, construction time, training time and prediction time by branch. These measurements are environment-local, not enforced equal-clock budgets; full memory accounting and cost-capped tuning are still outstanding.
- No claim of superiority is made when a braid feature fails to improve MAE, cost or calibration.

## Release judgment

**2.2 is an experimental numerical candidate release. Scientific numeric gates G0–G2 passed for these synthetic fixtures; G4 is a negative braid ablation; full G3 integrated BEAM release remains pending and must not be represented as passed.**

## SKYTRACE 2.2 — Numerical Release Candidate

I implemented the four comparisons in your release contract and ran the numerical tests. The results establish improvements in physical inference and reproducibility, while the Fibonacci-matrix augmentation remains a negative ablation.
SKYTRACE 2.2 — Research Bundle

Full repository · Numerical results · Clock inference · Fibonacci matrices · Protocol tests · Release gates

Download SKYTRACE 2.2

Read release contract · Numerical results

## 1. Repeated-seed uncertainty results

Five simulation seeds, each containing 40 held-out trajectories.

| Estimator | Mean MAE (rad/s) | 90% coverage | Mean interval width |
| -------------------- | ---------------- | ------------ | ------------------- |
| Single bearing | 0.07315 | 93.0% | 0.27598 |
| Multisensor | 0.00817 | 92.5% | 0.04192 |
| Multisensor + range | 0.00816 | 92.5% | 0.04189 |
| Fusion + braid signs | 0.00836 | 91.5% | 0.04079 |

The frozen SKYTRACE 2.1 MAEs reproduced with zero numerical difference in this environment.
These coverage results are encouraging, but five seeds are not enough to certify statistical calibration.

## 2. Joint clock-offset recovery

I introduced actual clock errors, keeping the physical target trajectory unchanged while shifting sensor-reported timestamps.
Unknown-clock offset MAE  
**5.69 ms**

Solver convergence failures  
**0 / 12**

| Measurement | Known clocks | Unknown clocks jointly estimated |
| ------------------------ | -------------- | -------------------------------- |
| Turn-rate absolute error | 0.002547 rad/s | 0.002480 rad/s |
| Trajectory RMSE | 0.0270 m | 0.0322 m |
| Mean solver evaluations | 5.17 | 6.42 |

This demonstrates feasible clock-offset recovery in the controlled 12-case fixture. The slightly smaller turn-rate error in the unknown-clock branch is sampling variation, not evidence that unknown clocks improve inference.

## 3. Fibonacci-matrix ablation

This experiment replaces the earlier braid-sign-only augmentation with matrix features from a two-dimensional Fibonacci braid representation.

| Branch                        | MAE (rad/s) | Feature count |
| ----------------------------- | ----------- | ------------- |
| Temporal baseline             | 0.008322    | 7             |
| Temporal + Fibonacci matrices | 0.008681    | 15            |

The matrix augmentation increases error by approximately 4.3% on this 20-case held-out ablation.
The braid relations and unitarity checks pass, but the representation has not demonstrated additional task-relevant value. It also costs more to construct.

## 4. Engineering verification

New 2.2 and retained 2.1 Python tests  
16/16 passed

Node evidence tests  
5/5 passed

NDJSON kernel protocol recovery  
Passed

BEAM/Elixir end-to-end runtime  
Not verified
The Python protocol successfully processed a valid request, rejected an invalid request, and processed another valid request without losing synchronization. BEAM worker source is included, but the Elixir runtime wasn’t available to validate real supervisor behavior, process memory, throughput, backpressure or restarts.
Release judgment: SKYTRACE 2.2 is a tested numerical research candidate, not yet a fully integrated BEAM release. The strongest result is multisensor physical inference with recoverable clock offsets. The braid experiment remains separately documented, exactly as your contract requires.
