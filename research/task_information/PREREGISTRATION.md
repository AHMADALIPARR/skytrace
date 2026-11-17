# SKYTRACE 1.7 — Task-relevant information benchmark

**Dataset:** synthetic angular tracks only. **No real GIMBAL or PURSUE sensor analysis in this run.** Seed `20261117`, 200 generated samples, 100 for train partition (unused by fixed decision rules) and 100 held out. Target `Θ` is sign of synthetic angular velocity. Measurements: 9 timestamps and 9 noisy bearing samples per origin, with additive Gaussian angular noise σ=0.002 radians. No metric-speed claim.

## Hypothesis and exact comparisons

H0: deterministic Artin/Fibonacci braid features do not improve inference relative to a matched time-aware graph summary. H1: a preregistered braid feature yields higher held-out accuracy or better calibrated uncertainty under matched budgets, with a positive confidence interval across seeds.

1. `full_attributed_graph`: includes all original sensor data, calibration, origins, and provenance; upper reference for what information can be retained.
2. `canonical_graph`: remove only an *identical* record, with equality over origin, calibration, uncertainty, times, angles and range. A new processing history or different measurement must remain distinct.
3. `lossy_absolute_magnitude_baseline`: unsigned angular-rate mean and its squared mean; the fixed demo threshold on unsigned mean is deliberately inadequate for direction inference.
4. `temporal_signed_rate_baseline`: signed mean angular rate, decision threshold zero.
5. `braid_abelianization`: sample-to-sample positive angular rate -> positive crossing, negative rate -> negative crossing; generator switches between σ₁, σ₂ at each sample index; decision is sign of total exponent. This is an *arbitrary encoding*, not yet a canonical topological mapping. Additional matrix trace included in feature output but not used by the fixed classifier.
6. `combined_temporal_braid`: same decision rule as the temporal baseline pending a separately preregistered, trained model. This is a neutral control, not evidence of enhancement.

## Acceptance gates

- **Duplication:** identical republication cannot change fingerprint, graph count after canonicalization, or predictions.
- **Differential evidence:** altered angular samples from the same origin must not be removed by deduplication.
- **Ordering:** input-record permutation does not change the canonical fingerprint; **sample ordering remains meaningful**.
- **Range:** deletion or change of range cannot silently masquerade as the same observation. Missing range blocks metric speed; known range alone remains insufficient without platform motion and calibration.
- **Baseline fairness:** no claim of braid added value unless it improves over time-aware baseline and feature-count/cost controlled alternatives on held-out seeds.
- **Statistical analysis:** future runs use confidence intervals, cross-seed replication, negative controls, and fixed computational budgets. A 1.0 result on a deliberately easy toy task is not scientific evidence of superiority.
- **Quantum entropy:** unitary conjugation is spectrally invariant. Any information claim derived from entropy of `UρU†` alone is rejected.

## Results

See `results.synthetic.json`. In the first demonstration, braid sign and the signed angular-rate baseline both obtain 1.00 held-out accuracy; no braid improvement is observed. The unsigned baseline scores 0.35 with its fixed threshold, demonstrating its inadequacy for this task rather than any unique braid advantage.

## Formal targets

For exact duplicates, `C(G ⊎ duplicate)=C(G)`. For commutative record insertion, `C(G₁ ⊎ G₂)=C(G₂ ⊎ G₁)` under canonical sorting. Preserve sample ordering inside observations. Express metadata-sensitive equality and duplicate identity as proof obligations in Lean; numerical unitary preservation is separately checked with independent eigensolvers. No theorem is claimed until a Lean/SAW proof log is obtained.

## Implementation note

`benchmark.py` is the offline reference model. In the BEAM pipeline, worker outputs must be merged and normalized before passing to this model or to a native numerical implementation with the same JSON contract. A worker may not independently deduplicate across partitions.
