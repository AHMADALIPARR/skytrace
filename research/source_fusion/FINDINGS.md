# SKYTRACE 2.0 — Source-metadata × estimator fusion

**Scope:** controlled synthetic fusion. `data/sources.json` contains public source-registry landing-page metadata for PURSUE, NARA RG615 and AARO. The source-to-trajectory attachment is **synthetic**, determined by trajectory ID modulo three and independent of turn rate. There are **zero original GIMBAL/PURSUE sensor traces** in this benchmark. Therefore, results are not case-level UAP findings.

## Inference and provenance boundary

- Immutable evidence provenance identifiers (`source_id`, source URL, source record ID, recording origin) remain in a separate sidecar; they are not numeric labels and are never features.
- Feature inclusion is explicitly limited to source category, telemetry presence, corroboration, and sensor noise metadata. Unknown is encoded as unknown, not false. The frozen fixture uses unknown telemetry/corroboration and constant sigma, so these fields provide no new predictive information.
- No real archive-to-sensor join is authorized by matching a title or timestamp alone. A real binding requires an original artifact digest, an exact source record key, an extraction provenance edge, calibration metadata, and explicit recording-origin identity.
- The braid representation is the existing SKYTRACE 1.8 deterministic word encoding. It does **not** estimate time exactly: it encodes crossing signs and loses rate magnitude. The braid+metric branch in 1.8 includes temporal features.

## Results

| Branch | Held-out MAE (rad/s) | Nominal 90% interval coverage |
|---|---:|---:|
| Temporal | 0.000733 | 85.7% |
| Temporal + source attributes | 0.000730 | 85.7% |
| Temporal + braid word | 0.000746 | 90.0% |
| Temporal + source + braid | 0.000749 | 90.0% |
| Source attributes only | 0.077191 | 95.7% |

The 0.000003 rad/s delta is not material evidence of added signal; no repeated-seed significance or paired confidence interval was performed. Time metrics are stored in `results.synthetic.json` and include feature building, ridge training/tuning and test inference, but represent a shared non-isolated host. Branch timings are too noisy here to establish a cost winner. These are synthetic outcomes, not forecasts for real GIMBAL/PURSUE observations.

## Next required real-source contract

```json
{
  "source_id": "pursue",
  "source_record_id": "OFFICIAL-ID-FROM-CATALOGUE",
  "original_artifact_sha256": "64-lowercase-hex-digest",
  "recording_origin_id": "verified-original-recording-id",
  "sensor_extraction_sha256": "64-lowercase-hex-digest",
  "sensor_type": "infrared-video",
  "observed_at": null,
  "time_uncertainty_ms": null,
  "range_state": "unknown",
  "range_m": null,
  "calibration_sha256": null,
  "extraction_method": "documented-versioned-toolchain"
}
```

This is a structural example, not a claim that any real case has these values. Block metric-speed reconstruction when range cannot be identified. Split by originating recording/case, never by frames, publisher or duplicated archive node.

## Acceptance tests

Run `python3 -m pytest -q research/source_fusion/test_fusion.py`. Checks source registry integrity, target independence, no unknown-as-false, duplicate-invariant features, changed-sample sensitivity, bad source URL rejection, and the existing braid collision regression.
