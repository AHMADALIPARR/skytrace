# SKYTRACE — evidence-first UAP research starter

This is a working **local evidence registry**, not a bulk crawler or a conclusion about alien origin. It has 10 curated entry points and reproducible SHA-256 evidence checks. It uses only Node.js built-in modules (Node >=20); no npm dependencies, cloud account, or database service.



## Added architecture artifact

- `spec/assets/skytrace-ingest-proof-flow.svg` — reference diagram for the archive ingestion and formal proof path.
- The build contract section **0A. Reference ingestion/proof topology** defines the semantics and interfaces attached to that SVG.


## Quick start

```sh
node --test tests/*.test.mjs
node bin/skytrace.mjs sources
node bin/skytrace.mjs cases
node bin/skytrace.mjs verify
```

## Add an official file you downloaded

```sh
node bin/skytrace.mjs ingest aaro PR-018 ./downloaded-video.mp4 'https://www.aaro.mil/UAP-Cases/Official-UAP-Imagery/'
node bin/skytrace.mjs verify
node bin/skytrace.mjs cases
```

The file is copied by SHA-256 content address into `data/raw/`, and a case record is created in `data/cases/`. Your original file remains unchanged. The same source ID and record ID always produce the same case ID. Each provenance entry records its source URL, SHA-256, byte size and ingestion timestamp. Local case IDs are *not* official case IDs.

`observed_at` starts as `null`, rather than guessing an incident date from the release date. `classification` starts as `unassessed`; being on an UAP portal does not make a case anomalous. Document claims separately from observations and model predictions.

## Research contract

1. Preserve originals, source URL, access timestamp, SHA-256 and byte size.
2. Never treat repeated uploads or syndication as independent witnesses.
3. Keep event time, report time, publication time and ingestion time distinct.
4. Record sensor calibration and unknown distance before estimating velocity.
5. Enumerate ordinary hypotheses before assigning unexplained residuals.
6. Admit findings by explicit tests and review; log falsifications as well as matches.
7. Never publish private witness details or sensitive observation locations without authorization.

## Planned modules (not included)

See `docs/RESEARCH_PROGRAM.md`. Signal processing, aerospace correlation, orbit propagation, probabilistic inference and formal proofs are an architecture specification, not features claimed to be present in this starter.


## Version 1.2: BEAM / OCaml / C / SAW architecture

- `beam/`: supervised archive snapshot ingestion and source adapters; requires Elixir and OTP 27+.
- `ocaml/`: strict NDJSON typed normalizer; requires OCaml, dune and yojson.
- `kernel/`: tested native C forest validation, active partition, and LLVM bitcode build.
- `proofs/saw/`: SAWScript against compiled C scalar decision predicates (requires external SAW and Z3).
- `fixtures/archives/`: synthetic input demonstrating source hierarchy. **Synthetic, not a government record.**

Native C tests and existing JavaScript/schema tests pass in the packaging environment. Elixir/OCaml/SAW are not installed in that environment and their compilation or solver proofs have not been validated here. Do not mark their evidence gates passing without CI artifacts.

## SKYTRACE 1.3 — sensor identifiability and probe hypothesis registry

- `research/GIMBAL_PURSUE_PROBE_PROGRAM.md` contains the proposed GIMBAL/PURSUE telemetry audit, source-by-source redaction criteria, physical identifiability tests, and autonomous-probe hypothesis test design.
- `research/hypotheses.json` contains the machine-readable preregistration of five competing models, four initial cases, telemetry states, and bibliography.
- `research/tests/test_hypotheses.py` checks registry consistency offline.

## 1.4 — Exploratory mathematics

`research/speculative_math/RESEARCH_CONTRACT.md` defines the Artin/Fibonacci braid representation, bounded DAG path unfolding, a GF(2) parity baseline, recursive probe populations, and an ER bridge scenario marked explicitly hypothetical. `core.py` implements finite computable models; `tests/test_core.py` checks their properties. This branch does not override the observational verification gates.

Run: `python3 -m unittest discover -s research/speculative_math/tests -v`

## 1.5 numerical closure / braid audit

Run `python3 research/numerical/analyze.py` to reproduce `research/numerical/results.synthetic.json`; see `research/numerical/NUMERICAL_FINDINGS.md` for the tri-state policy comparison, mathematical invariants and source-code audit. The values are from an explicitly synthetic archive fixture.

## SKYTRACE 1.6 — numerical sensor graph kernel

- `numerics/sensor_graph.py`: deterministic graph traversal (Kahn algorithm), recursive descendant observation aggregation, angular rate calculation, sensor-field fingerprints and independence accounting. JSONL input/output on stdin/stdout.
- `beam/lib/skytrace/numerical.ex`: bounded `Task.Supervisor` fragment ingestion and an Erlang Port calling the numerical kernel. Requires Elixir/OTP and Python on the target system.
- `fixtures/sensors/synthetic-graph.json`: clearly labeled synthetic test graph; not a UAP observation.
- `numerics/tests/test_sensor_graph.py`: adversarial and numerical tests.
- `research/numerical/SENSOR_GRAPH_FINDINGS.md`: numerical interpretation and inference boundaries.

Run the kernel independently:

```sh
python3 numerics/sensor_graph.py < fixtures/sensors/synthetic-graph.ndjson
python3 -m unittest discover -s numerics/tests -v
```

`Skytrace.Numerical.analyze(["/path/to/fragment.json"], "/path/to/numerics/sensor_graph.py")` sends merged graph fragments from BEAM workers to the Python numerical kernel. **No claim of successful Elixir integration is made until the Elixir toolchain is available and its tests run.**

## SKYTRACE 1.7 — Task-relevant information preservation

`research/task_information/PREREGISTRATION.md` sets target Θ, exact duplicate equivalence, graph/braid baselines, and the requirement for a matched time-aware baseline. `benchmark.py` generates the controlled synthetic data, compares fixed decision rules, and checks duplicate sensitivity. `results.synthetic.json` records the initial non-improvement result. Run `python3 -m unittest discover -s research/task_information/tests -v` and `python3 research/task_information/benchmark.py` (NumPy is required for the spectral test only).

## SKYTRACE 1.8 — physical parameter recovery benchmark

`research/parameter_recovery/` adds a synthetic constant-turn (rad/s) recovery benchmark, preregistration, ridge/trajectory model-fit baselines, braid encoding ablations, uncertainty coverage, source-duplication invariance, and frozen-model robustness tests. See `research/parameter_recovery/FINDINGS.md`.

## SKYTRACE 1.9 — graph baseline and budget probe

`research/graph_budget/` adds a full attributed time-ordered graph regression baseline to the frozen 1.8 synthetic task, collision/duplication regression checks, measured per-track construction+prediction timing, and an explicitly bounded cost protocol.

Run from the repository root:

```bash
OPENBLAS_NUM_THREADS=1 python3 -m unittest research.graph_budget.test_benchmark -v
OPENBLAS_NUM_THREADS=1 python3 -m research.graph_budget.benchmark
```

See `research/graph_budget/FINDINGS.md` and `PREREGISTRATION.md`.

## SKYTRACE 2.0 — provenance/estimator fusion

The `research/source_fusion/` branch joins source-*registry* metadata to the frozen synthetic 1.8 tracks for controlled ablation. See `research/source_fusion/FINDINGS.md`. It does not associate any real government case with synthetic observations. Run `python3 -m research.source_fusion.fusion` for JSON results; test using `python3 -m pytest -q research/source_fusion/test_fusion.py`.

## SKYTRACE 2.1 — coupled geometry/provenance benchmark

Run `python3 research/joint_fusion/benchmark.py` and `python3 -m pytest research/joint_fusion/test_benchmark.py`. See `research/joint_fusion/FINDINGS.md` for branch definitions and limitations. Generated synthetic sensor origins and instrument calibration are tied to the same simulated trajectory; source IDs are not shuffled class labels.


## SKYTRACE 2.2 — distinct release comparisons

- `research/release_22/benchmark.py`: frozen 2.1 reproduction, five-seed uncertainty summary, twelve-case joint clock inference, and independent Fibonacci matrix ablation.
- `research/release_22/kernel_port.py`: bounded one-line-in/one-line-out NDJSON protocol worker.
- `beam/lib/skytrace/release22.ex`: BEAM worker integration source; needs OTP runtime integration testing.
- `research/release_22/RELEASE_CONTRACT.md`: acceptance gates and limits.
- `research/release_22/results.synthetic.json`: numerical output with environment metadata.

Run from repository root: `python3 research/release_22/benchmark.py` and `python3 -m pytest -q research/release_22/test_release.py`.
