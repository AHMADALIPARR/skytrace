# SKYTRACE

SKYTRACE is an evidence-first research system for unidentified anomalous phenomena (UAP) records. It keeps official files intact, records where every artifact came from, reasons about provenance with explicit three-valued logic, and runs the physical and mathematical analyses that the research contract asks for. It is not a crawler, it does not decide that anything is of alien origin, and it does not turn a missing measurement into a measured zero.

The production runtime is ISO C11, BQN and SWI-Prolog. The supervised ingestion layer is Elixir/OTP and the typed NDJSON normalizer is OCaml. There is no JavaScript or Python in the native runtime. The earlier JavaScript CLI and the Python research code are still in the repository as reference material, and the parts that are not yet ported are listed under [Remaining reference code](#remaining-reference-code).

Every number in this README comes from a test log. Nothing here is an estimate.

## Contents

1. [Purpose and research contract](#purpose-and-research-contract)
2. [Architecture and computational ownership](#architecture-and-computational-ownership)
3. [Repository layout](#repository-layout)
4. [Build and test](#build-and-test)
5. [Command-line interface](#command-line-interface)
6. [The mathematics](#the-mathematics)
7. [Verification status](#verification-status)
8. [Measured results](#measured-results)
9. [Research history](#research-history)
10. [Remaining reference code](#remaining-reference-code)
11. [Limitations and epistemic rules](#limitations-and-epistemic-rules)
12. [License](#license)
13. [Credits](#credits)

## Purpose and research contract

SKYTRACE started as a local evidence registry for public UAP material: ten curated entry points (the PURSUE disclosure archive, AARO case records and imagery, NARA Record Group 615, the NASA independent study, CNES GEIPAN, NUFORC witness reports, OpenSky aviation data, CelesTrak orbital elements and others, all listed in `data/sources.json`), reproducible SHA-256 evidence checks, and case records that never overwrite the original file. It has since grown a provenance graph, a sensor-fusion kernel, a braid feature pipeline and a finite quantum model. The contract is unchanged:

1. Preserve originals, the source URL, the access timestamp, the SHA-256 digest and the byte size.
2. Never treat repeated uploads or syndication as independent witnesses.
3. Keep event time, report time, publication time and ingestion time distinct.
4. Record sensor calibration and unknown distance before estimating velocity.
5. Enumerate ordinary hypotheses before assigning unexplained residuals.
6. Admit findings by explicit tests and review, and log falsifications as well as matches.
7. Never publish private witness details or sensitive observation locations without authorization.

Two rules run through the whole codebase:

- **Unknown is not false and not zero.** Calibration, range, corroboration and resolution are three-valued (`known_true`, `known_false`, `unknown`). Missing telemetry stays `unknown` all the way through the C kernel, the Prolog rules and the reports.
- **Mathematical structure is not physical evidence.** Two trajectories with the same braid word are not the same trajectory, and Born-rule branch weights in the Alice–Bob model are not evidence for the many-worlds interpretation. Both statements are printed by the CLI next to the numbers they qualify.

New case records start with `observed_at: null` rather than a guessed date, and `classification: unassessed`, because appearing on a UAP portal does not make a case anomalous. Local case IDs (`ST-` plus the first 16 hex digits of a SHA-256 over the source and record IDs) are not official case IDs.

## Architecture and computational ownership

Each language owns one kind of work. Where two languages compute the same quantity, it is on purpose, as an independent check.

| Owner | Location | What it owns |
|---|---|---|
| **C11** | `include/skytrace/*.h`, `src/` | SHA-256 content addressing, the immutable evidence store, the hash-chained append-only journal, the strict JSON model, the CSR provenance graph and containment forest, the sensor-graph audit, the constant-turn nonlinear least-squares fit, the Fibonacci B₃ representation and exact Burau word equivalence, complex density matrices, partial traces, the Hermitian eigensystem and von Neumann entropy, CHSH, and the single `skytrace` CLI |
| **BQN** | `bqn/*.bqn`, `bqn/tests/run.bqn` | Array-oriented numerics: timestamp normalization and indexed temporal operations, coordinate transforms, angular velocity and acceleration, residual arrays, covariance and a cyclic Jacobi eigensolver, trajectory features, graph spectra, braid matrices and entropy/CHSH. It reads the C engine's typed interchange buffers and recomputes the results independently |
| **SWI-Prolog** | `prolog/skytrace.pl`, `prolog/json_schema.pl`, `prolog/tests/*.plt` | Semantic rules: three-valued provenance predicates with tabled recursive closures, the JSON Schema contract for `spec/schemas`, and the hypotheses-registry contract |
| **Elixir/OTP** | `beam/` | Supervised archive snapshot ingestion, the source adapters (PURSUE, NARA, AARO), and the Port integration that sends graph fragments to a numerical worker |
| **OCaml** | `ocaml/` | The strict typed NDJSON normalizer (`skytrace-normalize`) |
| **Original C kernel** | `kernel/` | The first native forest validator and active-partition predicates, kept unchanged with its own tests and bitcode build |
| **SAW and z3** | `proofs/saw/`, `proofs/smt/` | Machine-checked proofs over LLVM bitcode for the 32-bit case predicates and the Kleene logic, and bounded SMT lemmas |

### Data interchange

The C engine and BQN exchange arrays through **STBUF** buffers, a small versioned binary format documented in `bqn/interchange.bqn` and `include/skytrace/core.h`:

- bytes 0–7: the magic `STBUF\0\0\1`
- bytes 8–11: a `u32` element type (1 = `f64`, 2 = `i64`)
- bytes 12–15: a `u32` rank, at most 8
- then `rank × u64` for the shape, then the elements, all little-endian

The BQN reader validates every header field and the exact payload length before it casts anything, and it asserts that the host is little-endian. The C side writes them with `skytrace export <dir>`.

Records and journal entries are canonical JSON (RFC 8785 ordering), so the same input always gives the same digest.

## Repository layout

```text
skytrace/
├── CMakeLists.txt            native build and the 21 ctest targets
├── include/skytrace/         public C11 headers: core, error, evidence, graph, sensor,
│                             fusion, braid, quantum, entropy
├── src/                      core, evidence, graph, sensor, fusion, braid, quantum, cli
├── bqn/                      temporal, geometry, kinematics, covariance, spectral,
│   └── tests/run.bqn         braid, entropy, complex, interchange, assert
├── prolog/                   provenance rules, JSON Schema validator, plunit suites
├── beam/                     Elixir/OTP ingestion and adapters
├── ocaml/                    typed NDJSON normalizer
├── kernel/                   original C kernel, Makefile and tests
├── proofs/saw/, proofs/smt/  SAW scripts and z3 lemmas
├── tests/native/             C11 test programs (core, evidence, graph, sensor, fusion,
│                             braid, quantum, cli)
├── spec/                     BUILD_SPEC.md, JSON schemas, examples, ingest/proof diagram
├── fixtures/                 synthetic archive batch, sensor graph, reference outputs
├── data/sources.json         the curated source registry
├── research/                 preregistrations, findings, Garside/Unlambda normalizer,
│                             and the Python reference benchmarks
├── docs/                     research program and the extracted design documents
├── bin/, tests/core.test.mjs, package.json   the original JavaScript CLI (reference)
└── numerics/                 the original Python sensor-graph kernel (reference)
```

The design documents in `docs/` are `RESEARCH_PROGRAM.md`, `UAP_ARCHIVES_PROVENANCE_AND_BRAID_PIPELINE.md`, `GARSIDE_UNLAMBDA_NORMALIZER.md` and `SKYTRACE_2_2_NUMERICAL_RELEASE.md`. The full implementation contract is `spec/BUILD_SPEC.md`.

## Build and test

Requirements:

- a C11 compiler (gcc or clang) and CMake 3.16 or newer
- CBQN (CI pins commit `588753c83963f5b1e9247bfba9c16758da22a348`, built with `make o3 FFI=0`)
- SWI-Prolog 9 or newer with `plunit`, `pcre` and `http/json`
- optionally z3, clang and SAW 1.6 for the proof targets

Build and run everything:

```sh
cmake -S . -B build -DSKYTRACE_REQUIRE_BQN=ON -DSKYTRACE_REQUIRE_PROLOG=ON \
      -DSKYTRACE_REQUIRE_Z3=ON -DSKYTRACE_REQUIRE_SAW=ON
cmake --build build -j1
ctest --test-dir build -j1 --output-on-failure
make -C kernel test CC=gcc
```

The `SKYTRACE_REQUIRE_*` options make configuration fail when a tool is missing, so a test can never be reported as passing because it was silently skipped. The C code builds with `-Wall -Wextra -Werror -pedantic`.

CI (`.github/workflows/native.yml`) runs the same steps on `ubuntu-latest`: it installs cmake, gcc, clang, SWI-Prolog and z3, builds or restores CBQN at the pinned commit, downloads SAW 1.6 with its solvers, then configures, builds, runs `ctest -j1` and runs the original kernel tests.

The 21 ctest targets are the seven C11 module tests, the CLI transcript replay, three CLI checks (`braid-check`, `quantum-check`, `fit` against the Python reference), the BQN export and assertions, plunit, two z3 lemma files, three bitcode builds and two SAW proof scripts.

## Command-line interface

There is one native entry point, `skytrace`. The root directory is `$SKYTRACE_ROOT`, or the current directory if that is unset.

```sh
skytrace sources
skytrace ingest <source-id> <record-id> <local-file> <original-url>
skytrace cases
skytrace verify
skytrace analyze < fixtures/sensors/synthetic-graph.ndjson
skytrace records fixtures/archives/synthetic-batch.json
skytrace braid-check
skytrace braid-equiv <word1> <word2>
skytrace quantum-check
skytrace fit fixtures/reference/physical_fixture.seeds0-11.json research/release_22/results.synthetic.json
skytrace export <dir>
```

- `sources`, `ingest`, `cases` and `verify` keep the behavior of the original `bin/skytrace.mjs`, and `native_cli` checks their output against the recorded transcript. `ingest` copies the file into `data/raw/` under its SHA-256 name, never overwrites an existing object, writes the case record and appends an event to the journal. `verify` re-hashes every stored object and checks the whole journal chain.
- `analyze` reads NDJSON sensor-graph requests on stdin and writes one canonical response per line. Its output is byte-identical to the Python reference on the sensor fixtures.
- `records` validates a provenance batch; `braid-equiv` decides exact equivalence of two B₃ words; `fit` runs the constant-turn fit on the twelve-seed fixture against the Python 2.2 output; `export` writes the STBUF buffers.
- `braid-check` and `quantum-check` print JSON with each check's value, expected value, tolerance and pass flag, plus the interpretation note.

Errors go to stderr as `ERROR: <message>` with a non-zero exit code. Every failure carries a stable code (for example `E_CYCLE`, `E_DANGLING`, `E_BOUND`, `E_DIGEST`, `E_OVERFLOW`) defined in `include/skytrace/error.h`.

## The mathematics

### Provenance DAG versus containment forest

SKYTRACE keeps two graph structures and never mixes them up.

- The **provenance DAG** holds the edges `derived_from`, `depicts` and `calibrated_by`. A derived artifact can have many parents, so a node can have many incoming edges and there can be many root-to-leaf paths.
- The **containment forest** holds `contains` edges. Each node has at most one container, and the relation must be acyclic.

An acyclic graph does not give unique paths, so a DAG check is not a containment check. Both are stored in compressed sparse row form (`st_csr`). Building a graph rejects dangling endpoints, self loops and duplicate `(from, to, type)` edges. On top of that the C11 graph module provides:

- deterministic Kahn topological ordering (the ready node with the smallest label goes first), which fails with `E_CYCLE` and names a node on the cycle
- a separate containment check (at most one parent, no cycles)
- bounded reachability and ancestry, which stop with `E_BOUND` after a set number of visits
- bounded root-to-leaf path unfolding
- a canonical digest that does not change when nodes are relabelled or shuffled: SHA-256 over the sorted edge list plus the sorted isolated labels
- exact-digest deduplication

The evidence store is append-only by construction. Objects are named `<sha256>.<ext>`, and an existing object must already hash to the same digest. The journal at `data/manifests/events.ndjson` is a hash chain: each line is a canonical object with `seq`, `timestamp`, `actor`, `kind`, `payload`, `prev_event_sha256` and `event_sha256`. Appends take an exclusive lock, re-verify the tail and fsync.

The Prolog side states the same rules as logic: `active_case/1`, `source_record/3`, `derived_from/2`, `independent_origin/2`, `corroborated_case/1`, `telemetry_complete/1`, `physically_identifiable/2`, `valid_provenance_path/2` and `conflicting_observation/2`. The recursive closures (`contains_path/2`, `derives/2`, `origin/2`) are tabled, so queries terminate on finite graphs, cycles included. Two observations that trace back to the same recording origin are not independent, however many times they were re-uploaded.

### Constant-turn sensor fusion

The physical kernel fits calibrated planar constant-turn motion:

```text
dx/dt = v cos ψ(t)     dy/dt = v sin ψ(t)     dψ/dt = ω,   ψ(0) = ψ₀
p(t)  = p₀ + v t · sinc(ω t / 2) · [cos(ψ₀ + ω t / 2), sin(ψ₀ + ω t / 2)]
```

Sensor `i` reports at time `t_rep`; the physical time is `τ = t_rep − δᵢ`, where `δᵢ` is that sensor's clock offset. The observations are

```text
range    rᵢ = |p(τ) − sᵢ(τ)|              (only when a range was actually measured)
bearing  βᵢ = atan2(y − s_iy, x − s_ix)
```

Sensor 0 is the reference clock (`δ₀ = 0`). Without that choice a common shift of all clocks could not be told apart from a shift of the trajectory in time. The parameters are `x0, y0, v, ψ, ω` plus `δ₁ … δₙ`.

The solver is Levenberg–Marquardt with Marquardt diagonal scaling on whitened residuals, using an analytic Jacobian. The test suite checks that Jacobian against central differences. Each fit reports whether it converged, the iteration and function-evaluation counts, the stop reason, the residual norm, the condition number, the parameter covariance and which parameters are identifiable. Bearings-only data never has ranges made up for it. With a single bearings-only sensor the Fisher information matrix is singular, and the kernel reports that instead of returning a confident answer.

The BQN engine checks the same model independently: its constant-turn positions match the C `st_ct_position`, its chord speed matches `v·sinc(ωΔt/2)`, and its bearing-rate finite differences shrink by about a factor of four each time the step is halved, as a second-order scheme should.

### Fibonacci braids

The Artin braid group on three strands is

```text
B₃ = ⟨ σ₁, σ₂ | σ₁ σ₂ σ₁ = σ₂ σ₁ σ₂ ⟩
```

The Fibonacci anyon model has one non-trivial particle τ with the fusion rule `τ ⊗ τ = 1 ⊕ τ`. The fusion space of three τ anyons with total charge τ is two-dimensional, and the fusion-space dimensions follow the Fibonacci numbers. With the golden ratio `φ = (1 + √5)/2`:

```text
F = [[1/φ, 1/√φ], [1/√φ, −1/φ]]        R = diag(e^{−4πi/5}, e^{3πi/5})
ρ(σ₁) = R                              ρ(σ₂) = F R F
```

The checks are unitarity of both generators and of `F`, `F² = I`, unitarity of `R`, `φ² − φ − 1 = 0`, the braid relation, inverse generators (conjugate transposes) and word multiplication. Exact word equivalence does not use floating point at all: it uses the reduced Burau representation over `Z[t, t⁻¹]` with `int64` Laurent coefficients, which is faithful for B₃ and fails with `E_OVERFLOW` rather than wrapping. The Fibonacci matrices only give a necessary condition. For example, σ₁¹⁰ maps to the identity matrix but is not the identity braid.

Braid features come in four modes, and each feature set declares which sensor inputs it came from:

1. braid word only
2. braid word with timestamps
3. Fibonacci matrix representation
4. braid word with metric attributes

Crossings come from a declared rule rather than being inferred: headings of successive fused displacements are unwrapped, crossing `k` uses σ₁ for even `k` and σ₂ for odd `k`, and the inverse letter is used when the heading rate is negative. `st_braid_collision` flags the case where two feature sets are equal in their mode while the measured physical parameters differ by more than a tolerance. **Braid-word equivalence does not imply equal physical trajectories.**

`research/garside_unl.c` and the `.unl` files hold the Garside normal-form normalizer, which emits pure Unlambda for B₃ and B₄. They are described in `docs/GARSIDE_UNLAMBDA_NORMALIZER.md`.

### Bell state, CHSH and entropy

The quantum module is a finite mathematical model. The Bell state is

```text
|Φ⁺⟩ = (|00⟩ + |11⟩) / √2
```

with basis order `|ab⟩` at index `2a + b`. Measurement along angle θ in the X–Z plane uses `A(θ) = cos θ · Z + sin θ · X` with projectors `(I ± A)/2`. The module implements density matrices `ρ = |ψ⟩⟨ψ|`, validation (Hermitian, unit trace, positive semidefinite), state normalization, partial traces, local unitaries, joint measurement probabilities, correlations, CHSH

```text
S = E(a, b) − E(a, b′) + E(a′, b) + E(a′, b′),   a = 0, a′ = π/2, b = π/4, b′ = 3π/4
```

no-signaling checks (Alice's marginals do not depend on Bob's setting and the other way round) and branch-probability normalization.

Von Neumann entropy is `S(ρ) = −Σ λₖ log₂ λₖ` in bits. The eigenvalues come from an explicit C11 eigensystem: the Hermitian `H = X + iY` is embedded as the real symmetric matrix `[[X, −Y], [Y, X]]`, which has the same spectrum with every eigenvalue doubled, and that matrix is diagonalized by cyclic Jacobi. Eigenvalues within tolerance of zero count as zero, and a clearly negative eigenvalue is an error.

The expected values are `S(ρ_AB) = 0`, `S(ρ_A) = 1` bit and `CHSH = 2√2`. A Fibonacci braid word (σ₁ σ₂ σ₁⁻¹) is also applied to Alice's qubit, which is identified with the two-dimensional total-τ fusion space. As a local unitary it must leave `S(ρ_A)` at 1 bit, leave Bob's reduced state unchanged and keep no-signaling, and the tests check all three.

Branch weights here are Born-rule probabilities of the model. They are mathematical branching, not physical evidence for or against the many-worlds interpretation.

## Verification status

**Proved by SAW (over LLVM bitcode, with z3), 12 proof goals:**

- `proofs/saw/closure_checked.saw`, on the original kernel: `st_incomplete` and `st_active` meet their bit-vector specifications, and the closure property holds for all 32-bit `r, c, s`.
- `proofs/saw/rewrite.saw`, on `src/core/tri.c`: `st_case_incomplete`, `st_case_active`, `st_case_active_strict`, `st_tri_and`, `st_tri_or` and `st_tri_not` meet their specifications; strict-active implies kernel-active; the policy-mismatch region is characterized exactly; and both Kleene De Morgan laws hold.

**Checked by z3 (bounded SMT lemmas):**

- `containment_walk_n6.smt2`, for graphs of six nodes: a walk of n steps that never reaches the root marker must repeat a node (pigeonhole); a walk that returns to its start never reaches the marker; and the marker is absorbing. All three come back `unsat` as required.
- `kernel_policy_bv32.smt2`: strict implies kernel; the two policies differ exactly when `s` is neither 0 nor 1 and the case is incomplete; a resolved case is never active; known range plus corroboration closes a case under both policies. All four are `unsat`, and a sanity query confirms the mismatch region is non-empty (`sat`).

**Not proved:**

- `proofs/saw/closure.saw` (the original script) does not load under SAW 1.6. It uses `name <- do {...}` where `let` is needed, and Cryptol needs parentheses around `==` inside `&&`. `closure_checked.saw` is the corrected copy; the original is kept unchanged.
- The forest invariant of the original `st_validate` over graphs of arbitrary size is **not** proved. The SMT lemma covers six nodes only. The graph checks are tested, not proved.
- The JSON parser, evidence store, journal, fit, braid and quantum modules are covered by tests and cross-checks (C against BQN, C against the Python reference outputs), not by formal proofs.
- The Elixir and OCaml trees are not built or tested in CI. No claim of a working Elixir integration is made.

## Measured results

The values below are from the ctest output of CI run [44158913485](https://github.com/AHMADALIPARR/skytrace/actions/runs/44158913485) and the local ctest logs, which give the same values. Every check uses a tolerance of 1e-12 unless stated.

**Test totals**

| Suite | Result |
|---|---|
| ctest | 21/21 passed |
| `test_core` | 158 checks, 0 failures |
| `test_evidence` | 47 checks, 0 failures |
| `test_graph` | 47 checks, 0 failures |
| `test_sensor` | 38 checks, 0 failures |
| `test_fusion` | 48 checks, 0 failures |
| `test_braid` | 96 checks, 0 failures |
| `test_quantum` | 1094 checks, 0 failures |
| `test_cli` | 26 checks, 0 failures |
| BQN | 95 checks, 0 failures |
| plunit | 69 of 69 passed |
| original kernel | all assertions passed |

**Quantum (C11, `quantum-check`)**

| Quantity | Expected | Measured |
|---|---|---|
| S(ρ_AB) | 0 | 4.805139755722375e-16 bits |
| S(ρ_A) | 1 bit | 1 bit |
| S(ρ_B) | 1 bit | 1 bit |
| CHSH | 2√2 = 2.8284271247461903 | 2.8284271247461894 (error 8.9e-16) |
| trace ρ_AB | 1 | 0.9999999999999998 |
| no-signaling max deviation | 0 | 1.11e-16 |
| branch probability sum error | 0 | 3.33e-16 |
| S(ρ_A) after the braid on A | 1 bit | 1 bit |
| ρ_B change after the braid on A | 0 | 5.7e-17 |

The BQN engine gives S(ρ_AB) = 4.805139755722375e-16 bits, S(ρ_A) = S(ρ_B) = 1 bit and CHSH = 2.82842712474619.

**Braids (C11, `braid-check`)**

| Check | Error |
|---|---|
| braid relation σ₁σ₂σ₁ vs σ₂σ₁σ₂ | 1.57e-16 |
| unitarity σ₁ / σ₂ | 0 / 2.2e-16 |
| inverse σ₁ / σ₂ | 0 / 2.2e-16 |
| F involution, F unitarity | 1.1e-16 each |
| R unitarity, φ² − φ − 1 | 0 each |

The exact Burau checks give σ₁σ₂σ₁ = σ₂σ₁σ₂ (true) and σ₁σ₂ = σ₂σ₁ (false). Both the three-τ total-τ and the four-τ total-1 fusion spaces have dimension 2. BQN independently gives a braid-relation error of 1.24e-16.

**Fusion**

- The analytic Jacobian agrees with central differences to a maximum relative error of 2.347e-08.
- For a single bearings-only sensor the Fisher information eigenvalues run from −3.799e-13 to 1.899e+06, so the problem is singular and is reported as non-identifiable.
- `fit` on the twelve-seed fixture: all 24 fits (12 seeds × known and unknown clocks) converged and agree with the Python 2.2 reference within 1e-6. Aggregates with known clocks: trajectory RMSE 0.0270 m, ω error 0.00255 rad/s. With unknown clocks: trajectory RMSE 0.0322 m, ω error 0.00248 rad/s, clock MAE 0.00569 s. The fixture is synthetic.

**BQN numerics**

- Constant-turn position vs C `st_ct_position`: difference 0.
- Chord speed vs `v·sinc(ωΔt/2)`: 1.1e-14. Turn rate vs ω: 1.96e-14.
- Bearing-rate error against the analytic rate shrinks by 3.99994 and then 3.99998 per halving of Δt, consistent with second order.

## Research history

- **1.0.** The evidence registry, the contract in `spec/BUILD_SPEC.md`, the JSON schemas and the ingest/proof diagram in `spec/assets/`.
- **1.2.** The BEAM / OCaml / C / SAW architecture.
- **1.3.** Sensor identifiability and a registry of competing hypotheses (`research/GIMBAL_PURSUE_PROBE_PROGRAM.md`, `research/hypotheses.json`).
- **1.4.** Exploratory mathematics, including an explicitly hypothetical ER-bridge scenario (`research/speculative_math/`).
- **1.5.** The numerical closure and braid audit (`research/numerical/`).
- **1.6.** The numerical sensor-graph kernel, now ported to C11 as `skytrace analyze`.
- **1.7.** Task-relevant information preservation (`research/task_information/`).
- **1.8.** The synthetic constant-turn parameter-recovery benchmark (`research/parameter_recovery/`).
- **1.9.** The graph baseline and budget probe (`research/graph_budget/`).
- **2.0.** Provenance/estimator fusion (`research/source_fusion/`).
- **2.1.** The coupled geometry/provenance benchmark (`research/joint_fusion/`).
- **2.2.** Distinct release comparisons, joint clock inference and the Fibonacci ablation (`research/release_22/`, `docs/SKYTRACE_2_2_NUMERICAL_RELEASE.md`). The clock-inference fit is the one now reproduced by `skytrace fit`.
- **Native rewrite.** The C11 + BQN + SWI-Prolog runtime, the Garside/Unlambda normalizer and the native CI.

Every research dataset in the repository is synthetic and labeled as such. No synthetic observation is associated with a real government case.

## Remaining reference code

The native runtime does not need JavaScript or Python, but these files are still in the repository:

- **JavaScript (reference):** `bin/skytrace.mjs`, `tests/core.test.mjs` and `package.json`. The native CLI replaces them and `native_cli` checks against their recorded transcript (`fixtures/reference/cli_transcript.node.txt`).
- **Python ported to C11/Prolog:** `numerics/sensor_graph.py` and its test (now `skytrace analyze`), `spec/tests/schema_contract.py` and `research/tests/test_hypotheses.py` (now the Prolog `schema_contract` and `hypotheses_contract` suites), and the 2.2 clock-inference path of `research/release_22/benchmark.py` (now `skytrace fit`, compared with its output).
- **Python not yet ported:** the remaining benchmarks and their tests under `research/` — `graph_budget/`, `joint_fusion/`, `numerical/analyze.py`, `parameter_recovery/`, `release_22/` (the rest of the benchmark and `kernel_port.py`), `source_fusion/`, `speculative_math/` and `task_information/`. They are kept as the source of the reference numbers.

None of these files is used by the native build or by CI.

## Limitations and epistemic rules

- Angular rates are angular. Without a measured range, no linear velocity or acceleration is claimed.
- Results on synthetic fixtures show that the code does what it says. They do not show anything about real cases.
- A proof covers exactly what its script states. The verification section above lists what is not covered.

## License

SKYTRACE is dual licensed:

```text
SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
```

You may use, modify and distribute it under either the GNU Affero General Public License, version 3 only ([LICENSE-AGPL-3.0](LICENSE-AGPL-3.0)), or the SnapKitty Opaque License 1.0 ([LICENSE-SnapKitty-Opaque-1.0](LICENSE-SnapKitty-Opaque-1.0)), at your option. See [LICENSE](LICENSE). Each source file carries the SPDX identifier and the copyright line in its header.

## Credits

Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab.

Built by Ahmad Ali Parr ([AHMADALIPARR](https://github.com/AHMADALIPARR)) and the NO reply team. CBQN is by dzaima and contributors, SWI-Prolog by the SWI-Prolog team, SAW by Galois, and z3 by Microsoft Research. The public archives listed in `data/sources.json` belong to their publishers.
