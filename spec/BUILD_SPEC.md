# SKYTRACE 1.0 — implementation contract

**Version:** 1.0.0-draft | **Date:** 2026-11-17 | **Profile:** Evidence-first, local-first, verifiable research

## 0. Scope and baseline

This contract expands the existing `bin/skytrace.mjs`, which already supports `sources`, `ingest`, `cases`, and `verify`. The CLI uses Node.js built-ins and SHA-256 content-addressing. The expanded contracts described below are *acceptance specifications*, not claims that all modules are already installed. A source's presence in the registry establishes a collection target, not a harvested corpus.

**Verified upstreams:** PURSUE https://www.war.gov/UFO/ ; AARO official imagery https://www.aaro.mil/UAP-Cases/Official-UAP-Imagery/ ; NARA RG615 https://www.archives.gov/research/topics/uaps/rg-615 ; NASA UAP https://science.nasa.gov/uap/ ; OpenSky https://openskynetwork.github.io/opensky-api/ ; CelesTrak https://celestrak.org/NORAD/documentation/gp-data-formats.php ; JPL Fireball https://ssd-api.jpl.nasa.gov/doc/fireball.html . Confirm license and access terms per artifact before bulk collection. Non-public defense systems, restricted feeds, and witness identities are outside scope.


## 0A. Reference ingestion/proof topology

The following SVG is incorporated as a normative architecture artifact for the archive-to-proof path. Save and render `spec/assets/skytrace-ingest-proof-flow.svg` directly; do not redraw or restyle it in downstream documentation without versioning the asset.

```text
Active Archives (PURSUE / RG615 / AARO)
  -> BEAM Supervisor (OTP async ingest)
  -> OPAM Normalizer (OCaml ADT + Port)
  -> SAW Invariant Proof (SAWScript + SMT)
```

**Interpretation contract:**
- **Active Archives** maps to W1 source adapters and catalogue snapshots.
- **BEAM Supervisor** is the ingestion orchestration tier. It owns retry policy, bounded concurrency, and failure isolation. If implemented outside Elixir/Erlang, the replacement must preserve OTP-style supervision semantics: isolated worker failure, bounded mailbox/backpressure, and restart intensity controls.
- **OPAM Normalizer** is the typed normalization boundary. It converts source-specific records into algebraic data types before emitting canonical JSON objects accepted by `spec/schemas/*.schema.json`.
- **SAW Invariant Proof** is the formal gate for checking preservation properties across normalization and publication-critical transforms. It complements, rather than replaces, Lean/Z3/TLA+ gates defined later in this document.

**Minimum interface commitments for the topology:**

```ts
interface IngestSupervisor {
  submit(job: SourceFetchJob): Promise<{job_id:string}>;
  status(job_id:string): Promise<{state:'queued'|'running'|'failed'|'completed'; attempts:number; last_error_code:string|null}>;
  drain(source_id: SourceId): Promise<{completed:number; failed:number; quarantined:number}>;
}

interface NormalizedPort<TInput, TOutput> {
  decode(input: TInput): Promise<TOutput>;           // reject malformed source structures
  encodeCanonical(output: TOutput): Promise<string>; // RFC8785 JSON bytes as UTF-8 string
  schemaValidate(output: TOutput): Promise<{ok:true} | {ok:false; errors:string[]}>;
}

interface SawGateInput {
  transform_id: string;
  input_digest: SHA256;
  output_digest: SHA256;
  schema_version: string;
  invariant_set: string[];
}

interface SawGateOutput {
  verdict: 'pass'|'fail'|'indeterminate';
  checked_invariants: string[];
  counterexample_artifact_sha256: SHA256|null;
  proof_artifact_sha256: SHA256[];
}
```

**Required invariants for the SAW gate:**
1. Source identity fields survive normalization unchanged or with a documented reversible transform.
2. No output record can claim a public artifact hash that is absent from the evidence store.
3. Redaction transforms alter only approved fields and preserve digests for untouched byte artifacts.
4. A schema-valid normalized record cannot omit `source_id`, `source_record_id`, `retrieved_time`, or provenance edges.

## 1. Project layout and dependency rules

```text
skytrace/
  bin/skytrace.mjs                  # existing public CLI
  lib/core/{hash,canonical,clock,errors,atomic}.mjs
  lib/ingest/{source-registry,http-client,catalogue,download,parse}.mjs
  lib/adapters/{pursue,aaro,nara,geipan,opensky,celestrak,jpl-fireball}.mjs
  lib/cases/{events,observations,corroboration}.mjs
  lib/correlation/{aircraft,satellites,meteors,weather,ranking}.mjs
  lib/physics/{calibration,projection,tracking,trajectory,uncertainty}.mjs
  lib/claims/{claim,dependencies,verdict,publish}.mjs
  spec/schemas/*.schema.json
  spec/examples/*.json
  tests/{unit,contract,integration,mutation,golden}/
  julia/SkytracePhysics/{Project.toml,src,test}/
  lean/SkytraceProofs/{lakefile.toml,SkytraceProofs.lean}
  tla/{Ingestion.tla,Ingestion.cfg}
  data/{raw,cases,manifests,derived,quarantine}/
```

**Versioning:** schema `schema_version` uses `MAJOR.MINOR.PATCH`; breaking field/type/meaning changes increment MAJOR. All timestamps are UTC RFC3339 ending in `Z`, with independent `time_precision` and `time_uncertainty_ms`. SI units only; angles in radians; WGS84 positions in degrees and meters. `null` means unknown, never zero. Sort map keys recursively and encode JSON using UTF-8 with no insignificant whitespace before hashing canonical claims. Use JSON Canonicalization Scheme RFC8785 for cross-language canonicalization; hash bytes exactly for evidence. HTTP payload hash ≠ decoded media hash unless media bytes match.

**Storage:** `data/raw/sha256/<first-two-hex>/<64-hex>` with original bytes never overwritten. Append-only event journal `data/manifests/events.ndjson` with sorted fixed key order, event id, previous event hash, timestamp and actor. Index with SQLite (WAL) or DuckDB for query; index is disposable and rebuilt from journal. Quarantine invalid material rather than silently sanitizing. Use atomic temp-file write, fsync file, rename, fsync parent directory on supported POSIX file systems. For concurrent writers, a single journal lock/transaction is required.

## 2. Shared types and interfaces

```ts
// Signatures are language-neutral API contracts expressed in TypeScript notation.
// TypeScript is NOT required by the implementation.
type SHA256 = string; // lowercase hex /^[a-f0-9]{64}$/
type ISO8601 = string; // UTC RFC3339
type SourceId = 'pursue'|'aaro'|'nara'|'geipan'|'nuforc'|'opensky'|'celestrak'|'jpl_fireball'|'ams';
type Verdict = 'admitted'|'rejected'|'indeterminate';
type ObservationType = 'video'|'image'|'radar'|'radio'|'eyewitness'|'telemetry';
interface SourceRecord { source_id:SourceId; source_record_id:string; title:string; landing_url:string; media_urls:string[]; release_at:ISO8601|null; observed_at:ISO8601|null; rights:string|null; metadata:Record<string,unknown>; }
interface SourceAdapter {
  readonly id:SourceId;
  discover(input:{cursor:string|null;limit:number;since:ISO8601|null}):Promise<{records:SourceRecord[];next_cursor:string|null}>;
  resolve(record:SourceRecord):Promise<{artifacts:{url:string;media_type:string|null}[];policy:'download'|'manual_review'|'metadata_only'}>;
  normalize(record:SourceRecord):Promise<NormalizedSourceRecord>;
}
interface NormalizedSourceRecord { source_id:SourceId; source_record_id:string; event_time:ISO8601|null; published_time:ISO8601|null; retrieved_time:ISO8601; title:string; original_uri:string; related_artifact_sha256:SHA256[]; }
interface EvidenceStore { put(bytes:Uint8Array,meta:EvidenceMetadata):Promise<{sha256:SHA256;size_bytes:number;uri:string}>; get(sha256:SHA256):Promise<Uint8Array>; verify(sha256:SHA256):Promise<boolean>; }
interface CorrelationEngine { evaluate(o:Observation, candidates:KnownObject[], settings:CorrelationSettings):Promise<CorrelationResult[]>; }
interface PhysicsEngine { reconstruct(input:ReconstructionInput):Promise<ReconstructionResult>; }
interface ClaimGate { evaluate(claim:ScientificClaim, context:ClaimContext):Promise<{verdict:Verdict;reason_codes:string[];proof_artifacts:SHA256[]}>; }
```

Use error codes as stable machine-readable values: `E_SCHEMA`, `E_SOURCE_POLICY`, `E_RATE_LIMIT`, `E_HTTP`, `E_SIZE_LIMIT`, `E_DIGEST`, `E_CONFLICT`, `E_TIME`, `E_CALIBRATION`, `E_FRAME`, `E_RANGE_UNIDENTIFIABLE`, `E_INDEPENDENCE`, `E_TOLERANCE`, `E_PROOF`, `E_PUBLICATION`. Never catch-and-relabel an integrity error as a network retry.

## 3. W1 — PURSUE archive ingestion

### 3.1 Adapter rules

PURSUE source landing: `https://www.war.gov/UFO/`. The archive is a web catalogue, not a guaranteed stable public JSON API. Use an explicit **catalogue snapshot** workflow: fetch the publicly accessible HTML with an identifying User-Agent and conditional GET, save original HTML in the evidence store, extract published record rows and linked artifact URLs into a manifest, and require reviewer approval before new selectors are accepted. Never assume paginated HTML is exhaustive without a termination proof. Every row must retain agency, release number/date, document identifier, original title, and original URL if displayed. Track `first_seen`, `last_seen`, `seen_in_snapshot_sha256`, `withdrawn_or_missing` separately; missing from one crawl is not deletion. Snapshot differences must be reviewable. Do not guess a date for historical records.

`PursueAdapter.discover({cursor,limit,since})` cursor format is base64url JSON `{snapshot_sha256,offset}`; offset is zero-based into records sorted by `(source_record_id, landing_url)`. Reject a cursor with a different snapshot digest (`E_CONFLICT`). `resolve` returns `download` only for publicly linked content whose host is allowlisted (`war.gov` and approved media domains following human review), `manual_review` for ambiguous indirect links, `metadata_only` for missing file URLs. Metadata-only records still enter the catalogue.

**HTTP security contract:** TLS verification on; redirect max 3; DNS re-resolve on redirect; deny private/loopback/link-local IP targets; only HTTPS; host allowlist; no cookie harvesting, CAPTCHA bypass, robots bypass, or unauthorized endpoints; 10 MiB HTML ceiling and configurable artifact ceiling 1 GiB; streaming hash; timeouts 15s connect / 120s response; maximum 2 simultaneous fetches per source and 1 request per second until provider guidance dictates stricter limits. Respect HTTP 429 `Retry-After`, exponential delay with jitter and max 4 retries for transient 429/5xx; never retry 4xx other than 429. Evidence originals have no text normalization.

### 3.2 W1 commands and outputs

- `skytrace catalogue snapshot pursue --out data/manifests/pursue-YYYYMMDD.json`: deterministic manifest from fetched page snapshot, including selector version and page hashes.
- `skytrace catalogue diff --old <manifest> --new <manifest>`: outputs added, changed, missing rows with pre/post fingerprints.
- `skytrace acquire --manifest <path> --mode approved`: downloads explicitly approved URLs, streams SHA-256 bytes into CAS, logs HTTP status/content-type/ETag/Last-Modified, redirects and retrieval time.
- `skytrace integrity --manifest <path>`: verifies each archived artifact against CAS and journal; any missing or mismatch is fatal.

**Manifest row**: `record_key = SHA256(UTF8(source_id + NUL + source_record_id))`; if no stable official ID, use SHA256(source-specific canonical landing URL) and record basis `url_fallback`. Multiple versions of a publisher document share logical record ID but have different artifact hashes and acquisition records. An official later correction must not overwrite earlier bytes.

### 3.3 W1 tests and gates

- Same fixture snapshot fetched twice -> identical logical record set and stable record keys (retrieval metadata may differ).
- Duplicate rows in one snapshot -> one logical record plus collision audit entry.
- Same official ID with changed PDF -> two distinct SHA-256 artifacts, one linked record, `revision_detected` event.
- Empty or structurally changed HTML -> adapter fails `E_SCHEMA`, **zero deletions**.
- Redirect to localhost, HTTP URL, 2+ GiB Content-Length, malicious path traversal -> acquisition refused before file write.
- Abort after partial stream -> no committed CAS object and journal contains failure status only.
- Corrupt one stored byte -> `integrity` fails `E_DIGEST`.
- Gate W1: all downloaded records have source address, access time, byte length, verified SHA-256 and provenance edge; zero silent drops. Coverage is reported as `indexed_rows / rows_present_in_captured_pages`, never as percentage of all government records without a complete source manifest.

## 4. W2 — independently recorded case investigation

An **event** is a real-world candidate occurrence; a **report** is an account; an **observation** is a measurement; an **artifact** is byte evidence. `case_id` is not a synonym for official record id. A case may represent multiple reports of one event. Never merge reports solely because dates or locations are nearby.

**Case-link algorithm:** candidate pairs if temporal intervals overlap after uncertainty expansion and if geospatial separation meets explicit configurable radius, or if source documents cite one another. Candidate similarity score is a *review signal*, not a merge decision. Human-reviewed merge requires `same_event` edge carrying evidence hashes, reviewer, date, and confidence. Distinct news reproductions of the same video share one `recording_origin_id`. Recording origin is mandatory for independence claims; unknown origin cannot be asserted independent.

**Independence rule:** independent sensor channels require different physical collection chains or documented independence. Different exports/crops of one clip, an article quoting a witness, and an AI-generated transcription of the same account do not add independent evidence. For two sensors on one physical platform, store common-platform correlation and do not infer independence automatically.

**Case dossier output:** UTC event time interval, WGS84 region or unknown, all reports, all original artifacts, sensor provenance, per-observation calibration, data quality indicators, chain of custody, resolution status, alternative hypotheses and falsifying observations. Protect witness personal details; publication uses redaction overlays and retains private originals only with authorization. A document may make a claim without providing the measurements to test it.

**W2 tests:** (1) Two copies of identical video from distinct web hosts -> one recording origin; (2) incompatible UTC windows -> no automatic merge; (3) 90-second uncertainty plus 2-minute clock offset -> expanded overlap only, no forced equality; (4) two raw camera originals from independently maintained instruments -> eligible for independent-channel assessment; (5) report with no media -> preserved as testimony, never promoted to calibrated measurement; (6) redacted public export must not contain identifiers from designated private fields. W2 gate: zero asserted independent channels with shared origin, unknown chain, or missing provenance.

## 5. W3 — known-object correlation

### 5.1 Candidate source adapters

| Source | Acquisition contract | Limitations |
|---|---|---|
| OpenSky | Documented OAuth REST for appropriate windows or authenticated Trino historical queries; query UTC time/geo bounding box; preserve original response/hash/query/account entitlement | ADS-B/Mode S coverage is incomplete; no match is not absence of aircraft |
| CelesTrak | Fetch documented GP data in OMM format, preserve retrieval time and epoch; propagate with SGP4 | Newest orbital elements are not historically valid for arbitrary older events; require epoch window and sensitivity |
| JPL Fireball | `https://ssd-api.jpl.nasa.gov/fireball.api` with documented parameters; record API signature, fields, rows and source acquisition time | USG fireball detections are not a complete meteor census |
| AMS | Public fireball event records, manual or permitted bulk export with author attribution; retain witness/source independence status | Self-reports, not independently calibrated radar measurements |
| NOAA/ERA5 | Permitted atmospheric datasets for date, time, location, atmospheric model run and levels; archive exact query and data | Gridded modeled values have spatial/temporal uncertainties |

**Geometry:** Normalize source coordinates into WGS84, UTC and a known frame; convert aircraft geodetic -> Earth-centered Earth-fixed -> observer ENU; satellite TEME -> Earth-fixed with explicitly versioned time model, then ENU; compute azimuth convention north-clockwise `[0,2π)` and elevation `[-π/2,π/2]`. Do not compare a raw aircraft GPS course to the camera line of sight. Record range, timestamp interpolation, covariance and whether object is above modeled local horizon.

**Candidate comparison:** For each timestamp `t_i`, predicted unit vector `u_i` and measured unit vector `v_i`, angular error `α_i=acos(clamp(dot(u_i,v_i),-1,1))`. Apply point-spread and attitude uncertainty; require valid coverage. Score via robust weighted angular residual or a normalized log-likelihood with documented noise model. Background/no-match is an explicit hypothesis, not assigned probability zero. Ranking is not identification; a match needs predeclared time-and-angle thresholds plus evidence adequate for discrimination.

**Result schema:** `candidate_id`, `candidate_class`, `ephemeris_sha256`, `source_record_sha256`, `observed_interval`, `matched_samples`, `total_samples`, `angular_residual_rad`, `time_residual_s`, `coverage_status` (`covered|partial|unknown`), `likelihood_method`, `assumptions`, `decision` (`consistent|inconsistent|indeterminate`), `limitations`.

**W3 tests:** fixed observer and overhead object -> elevation π/2 within 1e-9 rad; azimuth wrap 359.9° vs 0.1° -> 0.2° residual, not 359.8°; source timestamp off by +3600 s -> reject unless timezone correction is documented; obsolete satellite element epoch -> mark uncertainty or indeterminate; simulated missing ADS-B coverage -> never claim aircraft absent; known star/satellite with high-consistency tracks -> match within predeclared angular tolerance; second independently implemented coordinate transform -> agreement under numeric tolerance.

**W3 gate:** no `identified` classification unless source epoch validity, coverage status, calibration, positive match test, alternatives and independent reviewer are recorded.

## 6. W4 — physics and kinematic reconstruction

### 6.1 Inputs and identifiability

`ReconstructionInput` includes: frame timestamps and uncertainty, image measurements `(u,v)` plus pixel covariance, camera intrinsics `K`, distortion coefficients and calibration target/procedure, camera extrinsic pose over time and uncertainty, shutter type/rolling shutter line time, frame of reference, optional independently measured range or stereo baseline, environmental refraction if meaningful, and declared motion model. Unknowns are represented by explicit nulls and missing-observation flags.

For an ideal pinhole camera, a world point `X` projects as `q ~ K R(t) (X - C(t))`, followed by calibrated distortion and pixel division. A single monocular bearing determines a **ray**, not metric range; `v_perp = r * angular_rate` only with range `r` established. Treat rotational parallax, camera motion and optical artifacts as testable alternatives. No metric acceleration claim from monocular imagery without sufficient independent metric constraints.

### 6.2 Solvers

- Intrinsic calibration: Brown–Conrady radial/tangential lens model; save original calibration images, parameters, uncertainty and reprojection residual distribution.
- Frame measurement: detect/track feature, preserve raw pixels, per-frame mask and confidence; no destructive enhancement on originals.
- Known-camera pose: propagate platform position/orientation using interpolation and covariance, disallow extrapolation beyond declared limit.
- Reconstruction: triangulation for synchronized calibrated multi-view bearings; nonlinear weighted least squares on 3D track with configurable Huber loss and numerical Jacobian checks; report rank of normal matrix and condition number.
- Uncertainty: Monte Carlo with fixed seed and declared sampling model (at least 10,000 draws for released confidence intervals unless analytically justified); include systematic parameter variations, not only pixel noise; provide 95% intervals and feasible sets.
- Model comparisons: constant position, constant velocity, maneuvering object, moving observer, rolling-shutter/lens artifact, reflection and known-object trajectories. Hold out at least 20% of frames by contiguous time blocks for prediction testing where sample size permits.

**W4 tests:** (1) pinhole projection known point -> <1e-9 normalized-coordinate error; (2) zero camera movement + known-depth constant-speed synthetic object -> recovered speed within 1% under noiseless conditions; (3) monocular-only range-null fixture -> `E_RANGE_UNIDENTIFIABLE` for metric speed; (4) known camera panning across static star -> reconstructs static astronomical source, no fictitious acceleration; (5) negative depth / behind camera -> invalid projection; (6) near-parallel stereo rays -> indeterminate due to conditioning; (7) covariance scaling by four -> confidence interval width approximately doubles in linear regime; (8) independent Julia/C++ or Julia/JS reference solver -> matching synthetic results to declared tolerances.

**W4 gate:** every speed/acceleration quote must include metric range source, coordinate frame, uncertainty interval, calibration digest, tracking digest, solver version, sample count and sensitivity report. Unidentifiable parameters must be printed `unknown`.

## 7. W5 — scientific claim verifier

### 7.1 Claim format

Each claim contains immutable canonical ID, description, evidence hashes, model and version, parameter units, predeclared null/alternative hypotheses, falsifiers, expected numeric bounds, code/toolchain digests, method of validation and assessor decisions. Numeric claims use `value`, `unit`, `lower_95`, `upper_95`, `confidence_method`; no implied zero uncertainty. Qualitative findings have specified conditions and test result references. Claims about extraterrestrial provenance require affirmative distinguishing evidence; a residual `unresolved` result alone cannot support that inference.

**Claim gates in order:**

1. `G0_SCHEMA`: validate all JSON Schema documents, identifiers and enums; additional fields forbidden in signed/hashed records.
2. `G1_PROVENANCE`: every claim artifact digest exists and matches bytes; every reported observation maps to a source artifact and retrieval event.
3. `G2_INDEPENDENCE`: no independent corroboration is claimed from shared/unknown origins.
4. `G3_TIME_FRAME`: UTC time interval, clock uncertainty, frames, SI units and transforms are explicit.
5. `G4_IDENTIFIABILITY`: structural parameter identifiability and numerical conditioning sufficient for quoted parameter.
6. `G5_REPRODUCIBILITY`: frozen toolchain, deterministic seed, fixtures, and reproducibility trace; repeated run matches tolerance.
7. `G6_FALSIFICATION`: alternatives and counterexamples have been tested, including planted bad data.
8. `G7_FORMAL`: applicable machine-checkable invariant/solver proofs pass; no assumed theorem or waiver silently marked proven.
9. `G8_REVIEW`: domain reviewer signature is associated with exact claim digest, not only a mutable document title.
10. `G9_PUBLICATION`: redaction, rights, citation and consent checks pass.

**Verdicts:** `admitted` iff all mandatory gates pass; `rejected` iff falsifying test establishes a contradiction or material integrity failure; `indeterminate` if evidence, permission, computation or uncertainty is insufficient. Never conflate `indeterminate` with `rejected`. Each gate emits JSON with `gate`, `verdict`, `reason_codes`, `inputs_sha256`, `toolchain_sha256`, `report_sha256`, and `timestamp`; do not mark a `pass` if tool absent.

### 7.2 Formal models

**Lean 4:** prove `angleNormalize` result is within `[0, 2π)` under defined finite inputs via real-arithmetic reference; prove content digests are compared as immutable identities in the abstract data model; prove state transition from `Unassessed` to `Admitted` requires all gates; prove independent-origin counting deduplicates origin IDs. Use `lake build` with pinned Lean and mathlib revisions; `sorry`, `admit`, `axiom`, `unsafe` forbidden inside the project proof namespace except documented external library axioms (audited separately). Theorems over ideal real arithmetic do not prove floating-point implementation accuracy; numerical differential tests are separately mandatory.

**Z3:** validate feasibility constraints, e.g. nonnegative range, bounded sensor error and time consistency. `unsat` is meaningful only for the *specified* model; always archive solver input SMT-LIB2, output, solver version and unsat core when available. For critical `unsat` conclusions require second independent formulation or proof-log verification; no 'unsat' interpreted as physically impossible without checking assumptions.

**TLA+:** specify ingestion states `Discovered -> Approved -> Downloading -> Hashed -> Committed` and failures/quarantine. Invariants: `Committed => hash_verified`; `Committed => source_record_exists`; immutable hash binding; two distinct raw byte strings never share storage key *within the collision-resistant hash assumption*; no publication before gates admitted. Model-check bounded concurrent interleavings (2 writers, 3 artifacts), crash before commit, duplicate fetch and retry.

**Mutation tests:** byte-flip media, fake second independent source of same recording, 1-hour timestamp offset, swapped coordinate axes, tampered ephemeris epoch, unbounded range, impossible residual, forged proof verdict, revoked reviewer approval. Every mutant must be caught by its assigned gate. Record mutation ID and expected gate.

**W5 release gate:** all required checks green; zero unresolved integrity mutations; machine-readable verdict bundle plus reproducibility manifest published alongside any report. A failed theorem or missing solver is a blocking `indeterminate`, not an inferred proof.

## 8. Data model and database tables

Machine-readable JSON schemas in `spec/schemas/`: `artifact.schema.json`, `event.schema.json`, `observation.schema.json`, `correlation.schema.json`, `reconstruction.schema.json`, `claim.schema.json`, `verdict.schema.json`. Use $defs for reusable digest, timestamp, uncertainty and SI units. SQLite relational index minimum: `sources(source_id PK, url, tier)`, `source_records(source_id,source_record_id PK,metadata_sha256)`, `artifacts(sha256 PK,size_bytes,media_type,cas_uri)`, `acquisitions(acquisition_id PK,artifact_sha256,record_key,retrieved_at,http_status)`, `events(event_id PK,interval_start,interval_end,location_json)`, `reports(report_id PK,event_id,source_record_key,recording_origin_id)`, `observations(observation_id PK,report_id,artifact_sha256,type,calibration_sha256)`, `correlations(result_id PK,observation_id,candidate_id,decision,report_sha256)`, `reconstructions(run_id PK,event_id,model,report_sha256)`, `claims(claim_id PK,claim_sha256,verdict)`, `claim_dependencies(claim_id,artifact_sha256)`, `gate_results(claim_id,gate,verdict,report_sha256)`. Enforce foreign keys; never use floating point for timestamp identifiers.

**Query indexes:** `events(interval_start,interval_end)`, `observations(artifact_sha256)`, `reports(recording_origin_id)`, `correlations(candidate_id,decision)`. In Parquet preserve raw telemetry without rounding; record source units and conversion transforms. Add a provenance graph layer using explicit directed edges `(subject, predicate, object, assertion_digest)`; allowed predicates `derived_from`, `depicts`, `same_event_as`, `shares_recording_origin`, `contradicts`, `supports`, `calibrated_by`, `computed_with`.

## 9. API/CLI surface

CLI commands have JSON output option `--format json` and non-zero codes: 2 bad args/schema, 3 inaccessible source, 4 hash conflict, 5 insufficient calibration, 6 gate failure, 7 permission issue. Commands:

```text
skytrace catalogue snapshot <source> --out <file>
skytrace catalogue diff --old <file> --new <file>
skytrace acquire --manifest <file> --mode approved
skytrace integrity --manifest <file>
skytrace case assemble --case <id> --reports <path>
skytrace correlate --observation <path> --controls <path> --out <file>
skytrace reconstruct --input <path> --out <file>
skytrace verify-claim --claim <path> --out <file>
skytrace publish --case <id> --claims <path> --out <directory>
```

Every subcommand must emit a `run_id`, tool version, input hashes, output hashes and machine-readable diagnostics, and be safe to re-run (content-idempotent). No hidden source downloads in `publish` or `verify-claim`.

## 10. Research fixture plan and CI acceptance

- **Fixture A (`same-video-twice`)**: same bytes linked by two publishers; report independence count exactly 1.
- **Fixture B (`monocular-no-range`)**: bearings plus camera pose but no metric distance; reconstruct returns `E_RANGE_UNIDENTIFIABLE` for speed/acceleration.
- **Fixture C (`satellite-known-track`)**: pinned historical orbital elements and observer; SGP4 result and azimuth/elevation compared with independent reference within 0.01 degrees for documented validity interval.
- **Fixture D (`aircraft-covered`)**: archived OpenSky track and calibrated camera fixture; consistent candidate only inside covered time span.
- **Fixture E (`meteor-known-event`)**: archived JPL fireball row with event timestamp and uncertainty; matched event is conditional on location and geometry, not time alone.
- **Fixture F (`adversarial-catalogue`)**: malformed HTML, duplicate IDs, URL escape/redirect, changed metadata, timed-out fetch and alternate media digest; zero silent drops.
- **Fixture G (`claims-mutation`)**: inject 9 mutations listed in W5; each independently trips intended gate.
- **Fixture H (`kinematics-ground-truth`)**: synthetic calibrated two-camera sequence with known 3D trajectory and separate static/panning control; validate numerical error and confidence coverage.

CI jobs: `node --test`, schema fixture validation, adapter fixture contract tests (offline), Julia unit/differential tests, Z3 SMT fixtures, `lake build`, TLC model check, media hash mutation suite, reproducibility rerun and citation/rights audit. CI must not depend on live network access; network sync is an explicit separately logged task. Pin Node/Julia/Lean/Z3/compiler versions and hashes in reproducibility manifest.

**Acceptance thresholds:** core unit tests 100% pass; all source adapter fixtures pass; all 9 mutations detected; every raw artifact checksum validates; scientific result rerun within declared numerical tolerance; all applicable formal gates pass; no published metric speed without identifiable range; no publication of unauthorized private witness data; 100% of published findings have dependency graph and citations.

## 11. Delivery phases and milestones

| Phase | Completion evidence |
|---|---|
| P0 foundation | Versioned schemas, CAS migration, append-only journal, offline tests, reproducibility manifest |
| P1 official archive | Snapshot/diff of PURSUE and AARO catalogue, row-level provenance and immutable artifact hashes |
| P2 case graph | Report/observation/origin linkage, deduplication and redaction tests |
| P3 control models | OpenSky, CelesTrak, JPL and meteor control adapters; independent geometry checks |
| P4 physics | Calibrated camera transforms, identifiability gates, synthetic reference trajectories, uncertainty intervals |
| P5 scientific release | Claim/verification bundle, formal proofs, mutation results, independently reviewed public case dossiers |

No phase is declared complete by line count or demos alone. Attach the exact CI run, toolchain digests, test cases and scientific evidence supporting the milestone.

## 12. Limitations and epistemic rules

Official publication does not establish a report's interpretation. AARO and NASA methods emphasize observation quality and possible mundane misidentification. Some records are document narratives without calibrated instrumentation; they can be indexed and analyzed as claims but not used to infer metric acceleration. The absence of an aircraft/satellite/meteor candidate in incomplete control datasets cannot prove the absence of a conventional object. Global research ingestion is ongoing, and collection completeness is stated only relative to captured source snapshots.

## 13. Added reference implementation — BEAM → OPAM → C/LLVM → SAW

The provided diagram is the primary topology for archival traversal. The checked-in SVG is `spec/assets/skytrace-ingest-proof-flow.svg`. The implementation files are:

| Component | Files | Contract |
|---|---|---|
| BEAM supervised ingestion | `beam/lib/skytrace/{application,ingest}.ex` | Bounded OTP task fan-out, source-ordered result, snapshot digest, framed NDJSON Port |
| OCaml typed normalizer | `ocaml/bin/main.ml` | Strict source/agency/canonical ID/epoch/MIME, tri-state telemetry, unique-parent batch forest, canonical record output |
| Native decision/forest kernel | `kernel/src/skytrace.{c,h}` | 32-bit active/incomplete predicates, duplicate-parent/cycle/dangling-parent validation, active index selection |
| SAW LLVM proof | `proofs/saw/closure.saw` | Verify actual C predicates against exact bitvector specifications, plus logical closure implication |

**Important mathematical distinction:** the containment hierarchy is a rooted forest. The broader evidence provenance graph is a DAG and can have multiple paths; a unique path is guaranteed only for forest containment. Telemetry `unknown` remains an indeterminate measurement state. `resolved=true` overrides the active selection; it does not delete archival source records. Aggregate release totals (for example 478 items) are discovery hints unless corroborated by a retained complete source snapshot. A SAW proof of scalar closure does not automatically prove the graph walk, archive completeness, or optical interpretations.

Run verification in this order: `make -C kernel test bitcode`; `cd ocaml && opam exec -- dune build`; `cd beam && mix test`; `saw proofs/saw/closure.saw`. Unavailable toolchains must be reported as unverified, not marked passing.
