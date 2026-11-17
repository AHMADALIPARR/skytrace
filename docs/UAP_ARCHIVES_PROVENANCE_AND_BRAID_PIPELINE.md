# UAP Archives, Provenance Invariants, and the Braid Feature Pipeline

Organized prose from `source.md`. Code blocks were extracted to the files linked below. Wording is unchanged; only markdown structure, list formatting, and trailing whitespace were cleaned up.

The two images pair a frame from the Navy's unresolved ++[Gimbal FLIR encounter](https://www.aaro.mil/uap-cases/official-uap-imagery/)++ (-2° elevation, SLAVE mode, LST 1688, LTD/R) with President Trump's February 19, 2026 Truth Social directive, which launched the Department of War's ++[PURSUE portal](https://www.war.gov/ufo/)++. Across all active U.S. government UAP repositories—PURSUE, ++[NARA Record Group 615](https://www.archives.gov/research/topics/uaps/rg-615)++, and ++[AARO's active archive](https://www.aaro.mil/Portals/136/PDFs/FY25%20UAP%20Annual%20Report/AARO_FY2025_Consolidated_Annual_Report_on_UAP.pdf)++—every record is traversed under a single structural invariant: an **append-only, acyclic provenance DAG** keyed by (Originating_Agency, Canonical_ID, Release_Epoch, Format_MIME) where cases stay "active" only while kinematic or multi-modal telemetry remains incomplete.

## Active UAP Archives & Metadata Schemas

| Archive | Active Scope (Nov 2026) | Traversal & Metadata Keys | Conclusion / Active Filter |
| -------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| DOW PURSUE | 478 files across Releases 01–07 (May 8 – Oct 23, 2026) | Record ID (DOW-UAP-*), Agency (11 agencies), Release, Incident Date, Location, Type (.pdf, .vid, .img, .aud) | Unresolved cases; sensor videos redact range telemetry (arXiv:2610.08692) |
| NARA RG 615 & Bulk Catalog | 9+ agency series (FAA, NRC, ODNI, OSD, NSA, State, FBI, USAF, CBP) + Blue Book | NAID (catalog-export-<NAID>.json), Level of Description (Series / File Unit / Item), Object URLs (AC 26.2024) | Digital-only rolling transfers; isolates non-attributed or pending accessions |
| AARO Imagery & FY25 Report | 32 official videos (PR-001–PR-018, Gimbal, FLIR, Go Fast, Flyby) | Case/PR ID, DVIDS Page, Combatant Command, Sensor Modality, Resolution Status | "Active archive" = insufficient data to assess performance characteristics |

## Cross-Archive Traversal Invariant

Across all three portals, metadata traversal preserves three formal properties that you can check mechanically:
* **Monotonic provenance (no in-place mutation):** New findings append a tranche (R01–R07), a child NAID, or a resolution paper rather than overwriting earlier records.
* **Acyclic hierarchical containment:** Agency → Series/Release → Canonical_ID forms a strict DAG with unique leaf identifiers and deterministic MIME types.
* **Active-archive closure predicate:** A record enters the active conclusion partition iff \neg \text{has\_range\_telemetry} \lor \neg \text{corroborated} (for example, Gimbal's uncorroborated IR track or PURSUE's redacted HUD range fields).

## Piping BEAM → OPAM → SAW

The pipeline diagram that appeared here is the existing file [spec/assets/skytrace-ingest-proof-flow.svg](../spec/assets/skytrace-ingest-proof-flow.svg). The SVG in this source is byte-identical to that file (source.md lines 17–66).

1. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
2. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
3. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
4. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
5. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
6. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
7. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
8. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
9. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
10. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
11. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
12. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
13. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
14. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
15. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
16. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
17. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
18. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
19. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
20. **Fan out archive ingestion on BEAM**
   *Elixir/OTP DynamicSupervisor*
   Run supervised Task.async_stream workers against ++[NARA's bulk JSON exports](https://www.archives.gov/research/catalog/catalog-bulk-downloads/uap-bulk-download)++ (catalog-export-*.json), the ++[PURSUE index](https://www.war.gov/ufo/)++, and ++[AARO's imagery table](https://www.aaro.mil/uap-cases/official-uap-imagery/)++. Filter out records already marked resolved (such as Resolved as Balloon or Birds) and stream the active partition over an Erlang Port as NDJSON.
21. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
22. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
23. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
24. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
25. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
26. **Normalize schemas in an OPAM OCaml binary**
   *yojson + angstrom*
   Parse the heterogeneous JSON/HTML rows into a single OCaml algebraic data type carrying (agency, canonical_id, epoch, mime, has_range_telemetry, corroborated). Reject any record missing a canonical ID or forming a parent-child cycle before serialization.
27. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
28. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
29. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
30. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
31. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
32. **Compile the traversal and closure kernel**
   *C / Rust / Cryptol target*
   Express the DAG-walk and active-archive closure predicate in C, Rust, or Cryptol so ++[SAW](https://tools.galois.com/saw)++ can symbolically execute the exact decision function rather than trusting ad-hoc scripts.
33. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.
34. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.
35. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.
36. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.
37. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.
38. **Prove the traversal invariant in SAW**
   *SAWScript + Yices/Z3*
   Load the bitcode or Cryptol module into saw (or drive it over SAW's RPC API) to discharge two SMT obligations: every reachable leaf has a unique, cycle-free provenance path, and every record routed to the conclusion set satisfies the incomplete-telemetry predicate.

## SKYTRACE 1.5 — Numerical provenance / closure / braid audit

### Reproduction

```sh
python3 research/numerical/analyze.py
python3 -m unittest discover -s research/numerical/tests -v
make -C kernel test
```

**results.synthetic.json is produced from fixtures/archives/synthetic-batch.json. All three fixture records are synthetic, not harvested PURSUE evidence. No real-world UAP frequency, performance, or origin inference follows from these results.**

### Canonical metadata mapping

**Input resolved, has_range_telemetry, and corroborated are three-valued: true -> 1, false -> 0, null -> -1. Do not coerce unknown to false. st_active in the existing C kernel treats resolution=-1 as not resolved; a stricter archival admission policy requires resolution=0.**

|Property                                          |Exact fixture result|
|--------------------------------------------------|-------------------:|
|Nodes                                             |3                   |
|Parent edges                                      |2                   |
|Roots                                             |1                   |
|Root-to-leaf paths                                |1                   |
|Maximum path depth in edges                       |2                   |
|Kernel-active fixture records                     |3                   |
|Strict-active fixture records                     |3                   |
|Unknown-range fixture records                     |2                   |
|Unknown-corroboration fixture records             |2                   |
|All ternary flag configurations                   |27                  |
|`st_active` qualifying configurations             |16                  |
|Strict policy qualifying configurations           |8                   |
|Configurations differing between policies         |8                   |
|Fibonacci braid relation maximum absolute residual|~1.57e-16           |

### Formal boundary and code audit

1. The supplied verify_dag_step(node_id,parent_id,depth,max_nodes) checks only bounds and node != parent: it cannot prove a graph is globally acyclic. A 2-cycle 0 -> 1 -> 0 satisfies its local predicate for both edges whenever max_nodes >= 3 and depth < max_nodes. The C st_validate actually walks parent pointers and rejects a cycle; the global property requires a separate SAW/model-checking obligation over the full array or an inductive topological-rank invariant.
2. The supplied OCaml check_dag_invariant computes updated_map but calls has_cycle on parent_map, which omits the proposed edge. For a fresh canonical_id, the new edge is therefore not included in the cycle check. Fix by passing updated_map to the traversal; the state also needs snapshot-aware duplicate handling and composite identity because canonical IDs legitimately repeat across versions and agencies.
3. The supplied OCaml decoder uses booleans and defaults cannot represent missing/unknown telemetry separately. Tri-state metadata is necessary to avoid treating unavailable range as confirmed absent.
4. The supplied Elixir server filters by descriptive status text before normalization; this can silently exclude metadata required for an append-only archive. Preserve all source rows; classify and partition only after normalization. verified_count is a count of returned ok records, not SMT-verified artifacts.
5. The supplied SAW script’s llvm_verify property depends on a compiled module whose symbols and integer widths match the script exactly. The existing SKYTRACE kernel/build/skytrace.bc exports st_active and st_validate, not the user-proposed is_active_conclusion and verify_dag_step symbols. Use the existing proofs/saw/closure.saw for st_active until the new targets are provided and checked. No SAW proof claim is made without the solver log.
6. The fixed fixture DAG is a containment forest, not a general multi-parent provenance DAG; the two structures should be modeled separately.

### DAG -> braid mapping requirements

**To derive a braid word from a graph without cherry-picking, declare (1) a stable vertex/edge ordering based on immutable metadata, (2) a canonical rule assigning signed braid generators, (3) proof that graph-isomorphic inputs yield equivalent braid invariants under the chosen canonicalization, and (4) mutation tests. The demo words [1,2,1] and [2,1,2] are mathematically equivalent under the Artin relation but are not inferred from the UAP fixture. The numerical residual validates only the Fibonacci representation.**

### Next measurable datasets

Once the archive’s actual source snapshots are acquired, compute case-level distributions of missing range, independent corroboration, resolution status, and provenance completeness. Reconstruct angular tracks only from verified sensor media. Keep missing and redacted telemetry distinct. The current source code and fixture do not provide observational values for statistical estimates of nonhuman technology or ER bridges.

The numerical results JSON fragment that appeared here is at [research/numerical/results.synthetic_from_md.json](../research/numerical/results.synthetic_from_md.json) (source.md lines 159–242). It is a truncated paste of `research/numerical/results.synthetic.json` (missing the opening `{"` prefix; every line has two trailing spaces).

For branch 3—the braid-derived feature pipeline—the expected flow is

\[
\boxed{
\bar G
\;\xrightarrow[\text{discrete}]{\text{braid extraction}}\;
b
\;\xrightarrow{\text{Fibonacci representation}}\;
U_b
\;\xrightarrow{\text{state construction}}\;
\psi_b\ \text{or}\ \rho_b
\;\xrightarrow{\text{feature extraction}}\;
z_b
\;\xrightarrow{\text{regressor}}\;
(\widehat\omega,\widehat s)
}
\]

The key gradient boundary is braid extraction: a hard crossing/order encoding does not provide a useful ordinary backpropagation path to the sensor coordinates. The downstream regressor can still be trained normally. Discrete order-selection operations have zero gradients almost everywhere; differentiable relaxations are a separate construction.[1]

Because we have not fixed the encoding or readout, the dimensions below describe a concrete candidate implementation, not an existing SKYTRACE specification.

## 1. Dimensions through the forward pass

Let:

- \(N\): minibatch size.
- \(n\): fixed number of braid strands.
- \(M\): maximum number of crossing events after padding.
- \(d\): dimension of the chosen Fibonacci fusion sector.
- \(K\): number of real-valued readout features.
- \(h\): regression-head hidden width.

Keep \(n\) and the fusion sector fixed in the first experiment so every example uses the same representation space.

| Stage | Per-example shape | Batched shape | Notes |
|---|---:|---:|---|
| Canonical graph | Variable nodes and edges | Packed graphs | Node count need not equal strand count |
| Braid generator indices | \(m\) integers | \(N\times M\) | \(i_j\in\{1,\ldots,n-1\}\) |
| Crossing signs | \(m\) signs | \(N\times M\) | \(e_j\in\{-1,+1\}\); separate padding mask |
| Optional event attributes | \(m\times a\) | \(N\times M\times a\) | For example, crossing times |
| Generator matrices | \(m\times d\times d\), complex | \(N\times M\times d\times d\) | Prefer indexed application rather than materializing all matrices |
| Accumulated operator \(U_b\) | \(d\times d\), complex | \(N\times d\times d\) | Optional if only one state is propagated |
| State vector \(\psi_b\) | \(d\), complex | \(N\times d\) | More compact than a density matrix |
| Density matrix \(\rho_b\) | \(d\times d\), complex | \(N\times d\times d\) | Optional |
| Readout \(z_b\) | \(K\), real | \(N\times K\) | Fixed observable expectations |
| Regression output | \(2\), real | \(N\times2\) | Mean turn rate and uncertainty parameter |

For Fibonacci anyons with \(F_0=0,F_1=1\), the sector dimensions are

\[
d=
\begin{cases}
F_{n-1},&\text{total charge }1,\\
F_n,&\text{total charge }\tau.
\end{cases}
\]

These dimensions follow from the fusion rule \(\tau\times\tau=1+\tau\); the fusion space is not an ordinary tensor product of \(n\) qubits.[2][3]

For example, choosing \(n=6\) and total charge \(\tau\) gives \(d=8\). The corresponding operator is \(8\times8\), while a propagated state has only eight complex amplitudes.

## 2. Braid word to state

Write the extracted word as

\[
b=\sigma_{i_1}^{e_1}\cdots\sigma_{i_m}^{e_m},
\qquad
R_j=R(\sigma_{i_j})^{e_j}.
\]

Declare the application-order convention explicitly. For sequential application \(R_1\) first,

\[
\psi_j=R_j\psi_{j-1},
\qquad
\psi_m=R_m\cdots R_1\psi_0.
\]

Each \(R_j\) is a \(d\times d\) unitary matrix in the chosen Fibonacci representation.[2][3]

For the proposed implementation, choose a fixed normalized \(\psi_0\). Then either:

\[
\rho_b=\psi_m\psi_m^\dagger,
\]

or avoid constructing \(\rho_b\) and compute readouts directly:

\[
z_k=\psi_m^\dagger O_k\psi_m,
\qquad k=1,\ldots,K,
\]

where each \(O_k\) is a fixed Hermitian observable. These features are real, up to numerical roundoff.

### Avoid the entropy dead end

For this particular pure-state construction,

\[
S(\rho_b)=0
\]

for every braid. More generally, if \(\rho_b=U_b\rho_0U_b^\dagger\), its spectrum—and therefore its entropy—does not change with the braid.

Thus the proposed branch should use state-sensitive readouts, not entropy alone. A maximally mixed initial state would also be uninformative under unitary conjugation because it remains unchanged.

## 3. Where gradients actually flow

With fixed braid extraction, fixed representation matrices, fixed initial state, and fixed observables:

The braid-feature pipeline diagram that appeared here is at [research/parameter_recovery/braid_feature_pipeline.txt](../research/parameter_recovery/braid_feature_pipeline.txt) (source.md lines 358–376).

Although the state propagation consists of differentiable linear operations, there is no trainable upstream parameter to update in this version. Computing \(\partial\mathcal L/\partial z\) does not make the discrete word differentiable.

For a concrete head,

\[
u=\operatorname{GELU}(W_1z+b_1),
\qquad
(\widehat\omega,\ell)=W_2u+b_2,
\]

the dimensions are

\[
W_1\in\mathbb R^{h\times K},
\qquad
W_2\in\mathbb R^{2\times h}.
\]

Set

\[
\widehat s=\operatorname{softplus}(\ell)+s_{\min}.
\]

A proposed Gaussian negative-log-likelihood loss is

\[
\mathcal L
=
\frac{(\omega-\widehat\omega)^2}{2\widehat s^2}
+\log\widehat s,
\]

up to an additive constant. Here \(\widehat\omega\) and \(\widehat s\) have units of \(\mathrm{rad\,s^{-1}}\). Predicting \(\widehat s\) does not itself establish uncertainty calibration.

## 4. What changes if upstream components are learned?

There are three distinct experiments:

| Variant | Gradient reach | Interpretation |
|---|---|---|
| Fixed braid features | Regression head only | Cleanest initial representation benchmark |
| Learnable initial state or readout | Through unitary propagation to those continuous parameters | Learned state/readout on a fixed braid encoding |
| Relaxed crossing/order extraction | Potentially into continuous sensor embeddings | A different, relaxed encoder requiring separate validation |

Differentiable sorting methods can replace some hard ordering operations with continuous surrogates. They do not automatically make the resulting object an exact braid-group representation.[1][4]

For example, a weighted average of generator matrices is generally not unitary. Any relaxation must therefore state which algebraic properties it preserves and how it is evaluated after returning to hard decisions.

## 5. Budget and information bottlenecks

For a straightforward dense implementation:

- State propagation costs \(O(md^2)\) per example.
- Explicit accumulation of the full operator costs \(O(md^3)\).
- A state requires \(O(d)\) complex storage; a density matrix requires \(O(d^2)\).
- Naive reverse-mode differentiation through trainable state propagation stores intermediate states, adding memory proportional to word length.

These are dense-operation counts; a structured generator implementation may do better. Since \(d\) grows with a Fibonacci number, strand count must be part of the computational-budget specification.[2]

## Sources

[1] [2006.16038] SoftSort: A Continuous Relaxation for the argsort Operator https://arxiv.org/abs/2006.16038
[2] A categorical presentation of quantum computation with anyons https://www.cs.ox.ac.uk/people/bob.coecke/AnyonsFinal
[3] A short introduction to Fibonacci anyon models https://arxiv.org/html/0902.3275v1
[4] Fast Differentiable Sorting and Ranking http://proceedings.mlr.press/v119/blondel20a/blondel20a.pdf
[5] One Method to Differentiate Them All: Ranking, Sorting and Top-k Selection https://icml.cc/virtual/2025/poster/46060
[6] Braid Groups, Hecke Algebras, Representations, and Anyons https://simons.berkeley.edu/sites/default/files/docs/5862/blass-fast.pdf
[7] Fibonacci anyons https://en.wikipedia.org/wiki/Fibonacci_anyons
[8] Fibonacci Anyons https://mohoushmand.com/quantum_book/foundations/hw/fibonacci_anyons.html
[9] Differentiable Sorting: Techniques & Applications https://www.emergentmind.com/topics/differentiable-sorting
[10] Fibonacci Anyons: Universal Topological Quantum Computation via Braiding https://aqora.io/papers/W7222888643
[11] Quantum Computing Modalities: Fibonacci Anyons https://postquantum.com/quantum-modalities/fibonacci-anyons/
[12] Fibonacci Anyons & Universal Topological Quantum Computation https://kevin-loo.github.io/mydocs/MP_Presentation_2021.pdf
[13] Differentiable Sorting and Ranking | google-research ... https://deepwiki.com/google-research/google-research/12.2-differentiable-sorting-and-ranking
[14] google-research/fast-soft-sort: Fast Differentiable Sorting and Ranking https://github.com/google-research/fast-soft-sort
[15] Differentiable parallel approximate sorting networks https://johnhw.github.io/differentiable_sorting/index.md.html

## Findings — SKYTRACE 1.8

**The measurements below are from a reproducible synthetic fixed-seed experiment, with 70 held-out trajectories. The bench uses direct position observations; no ranges, bearings, classified feeds, or UAP videos were used.**

|Branch                        |Test MAE (rad/s)|90% interval coverage|Full width (rad/s)|
|------------------------------|---------------:|--------------------:|-----------------:|
|Lossy graph summary           |0.07824         |97.1%                |0.3195            |
|Temporal                      |0.00073         |85.7%                |0.0031            |
|Braid word                    |0.02632         |88.6%                |0.1102            |
|Braid with timing             |0.02479         |95.7%                |0.1202            |
|Braid with metric attributes  |0.00075         |90.0%                |0.0032            |
|Direct model-fitting reference|0.00054         |not computed         |not computed      |

**Values are rounded from results.synthetic.json. Timings depend on the machine and should be regenerated locally rather than interpreted as portable resource comparisons. The braid-plus-metric branch contains the temporal features and should not be interpreted as independent improvement from topology.**

**Identifiability control: the deterministic braid word is identical for trajectories with turn rates 0.04 and 0.12 rad/s (same sampling cadence, no measurement noise), while the temporal features distinguish them. A braid-only estimator cannot output separate values for these two identical encodings.**

**Exact-record provenance duplication: canonicalization preserves predictions for all five branches when an identical recording is repeated. Different sensor measurements remain distinct records even if they share a source origin.**

### Frozen-model robustness (MAE in rad/s)

|Intervention                 |Temporal|Braid word|Braid + timing|Braid + metrics|
|-----------------------------|-------:|---------:|-------------:|--------------:|
|Base                         |0.0007  |0.0263    |0.0248        |0.0007         |
|Add 0.1 m Gaussian noise     |0.0020  |0.0462    |0.0451        |0.0020         |
|Jitter timestamps by 0.03 s  |0.0007  |0.0263    |0.0248        |0.0007         |
|Drop every fourth measurement|0.0007  |0.0299    |0.0294        |0.0007         |
|Halve observation duration   |0.0033  |0.0381    |0.0284        |0.0033         |

**Full precision is retained in robustness.synthetic.json.**

**Conclusion: standard temporal features recover turn rate much better than the braid-only encoding in this setting. The braid augmentation does not materially improve parameter recovery. Real next milestones: calibrated range-bearing models, physical sensor synchronization, full graph estimator, proper matched compute caps, coverage assessment across repeated trials, and formal proof of the duplicate-canonicalization equivalence relation.**
