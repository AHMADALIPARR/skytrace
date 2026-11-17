# SKYTRACE 1.3 — GIMBAL / PURSUE / autonomous probe investigation

## Status of evidence

AARO lists GIMBAL among unresolved official imagery. NAVAIR hosts the original public video. A HUD field is not automatically calibrated telemetry: SLAVE indicates a targeting configuration; LST 1688 is a laser code, not target distance. Missing displayed range can mean not recorded, omitted during video export, outside the crop, masked, or explicitly redacted; these states **must never be conflated**.

Haqq-Misra and Kopparapu (2026), arXiv:2608.12445, analyze 112 PURSUE sensor videos and report that no examined video contains the complete information required to reconstruct absolute velocity. They identify PR113 (depression-angle calibration) and PR149 (reference object) as partial constraints. Haqq-Misra et al. (2026), arXiv:2610.08692, report independent 7.1–7.9 Hz brightness periodicities for six objects and interpret them as consistent with bird wingbeat rates. These are published analyses to replicate, not verified object identifications within SKYTRACE.

## Work package S1: Pixel/HUD evidence registry

For each case, preserve original clip bytes and SHA-256, authority landing URL, clip timebase, frame count, codec metadata, and all HUD regions as region-of-interest annotations. A telemetry field has `{name, value, unit, provenance_frames, state, confidence, derivation}`. Permitted states: `measured`, `displayed`, `obscured_confirmed`, `not_in_public_video`, `not_applicable`, `unknown`. `redacted` classification requires affirmative visual or documentary evidence of deliberate masking.

## Work package S2: Camera calibration and velocity identifiability

Given image-plane motion `v_px` px/frame, frame rate `f` frames/s and local pixels-per-radian scale `k`, compute line-of-sight angular rate `omega = v_px * f / k`. Ground-relative transverse velocity requires target range and platform velocity; total relative 3D velocity also requires radial rate. If any required input is unbounded, issue `UNIDENTIFIABLE` instead of a point estimate. For any given `omega > 0`, `R=100 m` and `R=1000 m` give transverse speeds differing by 10×, absent independent range constraints.

Test fixtures: stationary camera/static distant object; panning camera/static object; camera rotation causing image rotation; near bird/far craft degeneracy; reference ship of known dimensions; GIMBAL analog showing SLAVE/LST laser code but no range. Mutation: rename LST 1688 as 1688 m → must reject under `E_TELEMETRY_SEMANTICS`.

## Work package S3: PURSUE redaction audit

Register every public video and compare (a) the released frame, (b) accompanying record text, (c) versions across tranches. Detect visually blanked HUD regions, overlays, frame crops and absent data separately. Compare overlays from the same instrument family cautiously; absence of a HUD field is not positive proof of redaction. Produce source/clip-level matrix with `range`, `range_rate`, `azimuth`, `elevation`, `platform_position`, `platform_velocity`, `FOV`, `timestamp`, `radar_corroboration`. The target is to test whether physically identifiable trajectories survive publication, **not** to infer motive from missingness.

## Work package S4: Von Neumann/autonomous-probe literature map

'Von Neumann probe' is a theoretical self-reproducing exploratory machine, not an observed class of UAP. Related ideas: Bracewell communicating probes; autonomous nonreplicating reconnaissance probes; interstellar self-replicators; microscale replicators; asteroid/co-orbital 'lurkers'; solar-system artifact searches; theoretical galactic expansion; and terrestrial-origin speculation. Track each separately so that mere autonomous motion is not mistaken for replication.

Literature to review: Matloff (2022), *Von Neumann probes: rationale, propulsion, interstellar transfer timing*; Ellery (2022), *Self-replicating probes are imminent – implications for SETI*; *On the interstellar Von Neumann micro self-reproducing probes* (2019); *Lotka–Volterra models for extraterrestrial self-replicating probes* (2022); *A cost-effective search for extraterrestrial probes in the Solar system* (2025); Schmidt and Frank (2018/2019), *The Silurian Hypothesis*. See `hypotheses.json` for source links.

## Work package S5: Terrestrial-AI model testing

Separate (1) mundane objects/sensor effects, (2) human-engineered autonomous vehicles, (3) speculative ancient terrestrial machine lineage, (4) extraterrestrial autonomous probes, (5) self-replication. All alternative models must be pre-registered before applying to a test set. Evidence for intelligence requires engineered signaling, independent maneuver reconstruction, manufactured material analysis, or other genuinely discriminating measurements; tracking an unidentified dot is insufficient.

For ancient terrestrial industrial activity, geological/geochemical searches should look for co-located, securely dated evidence and explicitly account for known natural analogues. The Silurian hypothesis is a methodological thought experiment, not proof that such a civilization existed.

## Reproducibility gates

G-S1 original footage digest and rights; G-S2 frame/timebase alignment; G-S3 HUD annotation provenance; G-S4 calibration; G-S5 missingness vs redaction provenance; G-S6 identifiability; G-S7 adversarial controls; G-S8 hypothesis model comparison with stated priors; G-S9 independent replication; G-S10 reviewed publication. No case may graduate to an intelligence claim solely because conventional explanations have not yet been established.
