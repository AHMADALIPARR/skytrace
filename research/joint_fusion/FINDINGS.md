# SKYTRACE 2.1 — Physically coupled source/sensor fusion

The synthetic data generator couples the same planar constant-turn target to three spatially separated bearing instruments. Each instrument has a unique recording origin, source ID, sensor calibration, clock offset and timestamped raw observations; the central instrument also has noisy range measurements. Record duplication is canonicalized by exact JSON identity. No real UAP video was analyzed.

**Design:** 180 simulated trajectories; 100 train / 40 validation / 40 held-out test, random seed 20261117. Targets are turn rates in rad/s. Independent initial positions, headings, speeds, and sampling intervals are varied. Three sensor positions are `(-22,-12), (24,-9), (-7,27)` meters. Bearing noise standard deviation: 0.008 rad, range noise: 0.22 m, optical drop probability: 25%; independent clock offsets lie in ±0.12 seconds and the provided timestamps include those offsets.

**Computation:** intersect each sensor's valid timestamp coverage, interpolate unwrapped angles onto 12 evaluation times, triangulate from bearing lines by weighted least squares; optionally add radial range constraints. Fit ridge regressions to temporal kinematic summaries. Braid augmentation adds discrete alternating-generator crossing-sign statistics to **the same** fused metric features. Do not interpret this as full Artin/Fibonacci matrix-based inference.

| Branch | Test MAE rad/s | 90% coverage | Feature count |
|---|---:|---:|---:|
| Single-bearing | 0.081652 | 0.975 | 6 |
| Three bearings | 0.007551 | 1.000 | 7 |
| Three bearings + range | 0.007544 | 1.000 | 7 |
| Fused + braid-sign statistics | 0.007625 | 0.975 | 12 |

**Interpretation:** multisensor geometry improves recovery greatly over a **single-bearing** temporal baseline. The added noisy range term yields little incremental value in this configuration; braid-sign features do not improve recovered turn rate. Conservative interval estimates overcover; calibration needs more trials. These results do not compare against an optimal physical maximum-likelihood estimator. They are not evidence of UAP motion or spacecraft capabilities.

**Cost:** `results.synthetic.json` records wall times for complete feature extraction and ridge tuning, and Python `tracemalloc` peak (which does not include all native NumPy allocations). Figures are diagnostic, not equal-budget latency comparisons. Do not claim OOM resistance based on tracemalloc alone.

**Important limitations:** partial bearings are interpolated, no explicit estimation of unknown clock offsets (offsets are included in supplied timestamps), geometric range is synthetic, and sensor positions are assumed exact. The synthetic source IDs are not original PURSUE/AARO/NARA artifact hashes. Actual UAP fusion must await verifiable original video, instrument geometry and appropriate calibration. BEAM live worker integration is not part of this benchmark; it provides a bounded batch/record contract compatible with the existing architecture.
