# SKYTRACE 1.6 — Recursive provenance graph / sensor invariant results

## Mathematical contract

Given directed graph `G=(V,E)` with labelled containment/derivation edges, a set of observation records `O`, and an observation attachment `a: O -> V`, derive the following:

- Acyclicity: topological order exists for the **full** edge set. Parent uniqueness is additionally required **only** for `contains` edges.
- Reachable observations: `R(v) = local(v) union Union_{(v,w) in E} R(w)`. Dynamic programming over reverse topological order. `R` is a set, so a diamond join does not duplicate an observation.
- Angular rates from samples `(t_i, az_i, el_i)`: `omega_az = wrap(az_i-az_{i-1})/(t_i-t_{i-1})`, `omega_el = (el_i-el_{i-1})/(t_i-t_{i-1})`; `omega_norm = hypot(omega_az, omega_el)` in rad/s. Requires strictly increasing times, finite angles, non-ambiguous azimuth unwrap.
- Sensor semantic digest: `SHA256(canonical_JSON(observation_id, recording_origin_id, modality, calibration_sha256, range_m, samples))`. It is stable under record ordering and graph vertex renaming, **not a cryptographic proof of sensor authenticity or physical significance**. JSON normalization preserves numbers as provided, so semantically equal but differently represented numeric values can hash differently.
- Independent recordings: number of **distinct declared** `recording_origin_id` values, not number of files; source authenticity and shared-origin inference require external evidence.
- Range identifiability: missing target range implies no inferred metric speed. Even with range, calibration and platform motion must be supplied for a metric-velocity claim; this kernel does not calculate a metric velocity.

## Synthetic experimental results

The fixture intentionally has one archive root, one case, two recording records, and a diamond-shaped dependency (two paths to record-b).

| Quantity | Measured result |
|---|---:|
| Nodes | 4 |
| Edges | 4 |
| Reachable observations at archive root | 2 |
| Observation records | 2 |
| Unique declared recording origins | 1 |
| Missing target range | 2 |
| Calibrated observations | 0 |
| Angular rate of both tracks | ~0.020 rad/s |
| Recoverable metric target velocities | 0 |

## Adversarial checks

Tests confirm: edge/node/observation ordering does not change output; duplicate traversal paths do not duplicate observations; changing declared origin ID or dropping a sample changes the semantic fingerprint; two-node directed cycle is rejected; second containment parent is rejected; reversed timestamps, non-finite/invalid angular values, missing calibration-state key, and zero range are rejected.

## What the kernel preserves — and what it does not

**Preserved by design:** declared temporal order, angular sample values, modality, calibration identity/state, range identity/state, declared recording origin, and set-based graph reachability.

**Not established:** actual sensor calibration, true target range, target speed, independent origin authenticity, statistical evidence of anomalous propulsion, sensor noise model, or an invariant relating Artin/Fibonacci braids to physical data. Braid mapping is intentionally not invoked by this kernel until a mathematically justified structure-preserving map exists.

## Research next acceptance tests

1. Add original byte-level hashes of GIMBAL and PURSUE media to the observation artifact records, keeping published video and source telemetry distinct.
2. Recover pixel tracks from version-locked media; attach timestamps and projection/calibration uncertainty, not assumed values.
3. Compare graph transformations: lossless serializer, ordered vs unordered DAG traversal, dropped frame, duplicated recording, swapped modality, missing range, altered clock.
4. Define paired observations sharing independently corroborated real recording origins, then test whether the independence count remains valid.
5. Introduce full camera pose and uncertainty estimation before physical velocity inference.
