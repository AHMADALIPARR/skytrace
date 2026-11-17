# SKYTRACE 1.5 — Numerical provenance / closure / braid audit

## Reproduction

```sh
python3 research/numerical/analyze.py
python3 -m unittest discover -s research/numerical/tests -v
make -C kernel test
```

`results.synthetic.json` is produced from `fixtures/archives/synthetic-batch.json`. **All three fixture records are synthetic, not harvested PURSUE evidence.** No real-world UAP frequency, performance, or origin inference follows from these results.

## Canonical metadata mapping

Input `resolved`, `has_range_telemetry`, and `corroborated` are three-valued: `true -> 1`, `false -> 0`, `null -> -1`. Do not coerce unknown to false. `st_active` in the existing C kernel treats `resolution=-1` as not resolved; a stricter archival **admission** policy requires `resolution=0`.

| Property | Exact fixture result |
|---|---:|
| Nodes | 3 |
| Parent edges | 2 |
| Roots | 1 |
| Root-to-leaf paths | 1 |
| Maximum path depth in edges | 2 |
| Kernel-active fixture records | 3 |
| Strict-active fixture records | 3 |
| Unknown-range fixture records | 2 |
| Unknown-corroboration fixture records | 2 |
| All ternary flag configurations | 27 |
| `st_active` qualifying configurations | 16 |
| Strict policy qualifying configurations | 8 |
| Configurations differing between policies | 8 |
| Fibonacci braid relation maximum absolute residual | ~1.57e-16 |

## Formal boundary and code audit

1. The supplied `verify_dag_step(node_id,parent_id,depth,max_nodes)` checks only bounds and `node != parent`: it cannot prove a graph is globally acyclic. A 2-cycle `0 -> 1 -> 0` satisfies its local predicate for both edges whenever `max_nodes >= 3` and `depth < max_nodes`. The C `st_validate` actually walks parent pointers and rejects a cycle; the global property requires a separate SAW/model-checking obligation over the full array or an inductive topological-rank invariant.
2. The supplied OCaml `check_dag_invariant` computes `updated_map` but calls `has_cycle` on `parent_map`, which omits the proposed edge. For a fresh `canonical_id`, the new edge is therefore not included in the cycle check. Fix by passing `updated_map` to the traversal; the state also needs snapshot-aware duplicate handling and composite identity because canonical IDs legitimately repeat across versions and agencies.
3. The supplied OCaml decoder uses booleans and defaults cannot represent missing/unknown telemetry separately. Tri-state metadata is necessary to avoid treating unavailable range as confirmed absent.
4. The supplied Elixir server filters by descriptive status text before normalization; this can silently exclude metadata required for an append-only archive. Preserve all source rows; classify and partition only after normalization. `verified_count` is a count of returned `ok` records, not SMT-verified artifacts.
5. The supplied SAW script's `llvm_verify` property depends on a compiled module whose symbols and integer widths match the script exactly. The existing SKYTRACE `kernel/build/skytrace.bc` exports `st_active` and `st_validate`, not the user-proposed `is_active_conclusion` and `verify_dag_step` symbols. Use the existing `proofs/saw/closure.saw` for `st_active` until the new targets are provided and checked. No SAW proof claim is made without the solver log.
6. The fixed fixture DAG is a containment forest, not a general multi-parent provenance DAG; the two structures should be modeled separately.

## DAG -> braid mapping requirements

To derive a braid word from a graph without cherry-picking, declare (1) a stable vertex/edge ordering based on immutable metadata, (2) a canonical rule assigning signed braid generators, (3) proof that graph-isomorphic inputs yield equivalent braid invariants under the chosen canonicalization, and (4) mutation tests. The demo words `[1,2,1]` and `[2,1,2]` are mathematically equivalent under the Artin relation but **are not inferred from the UAP fixture**. The numerical residual validates only the Fibonacci representation.

## Next measurable datasets

Once the archive's actual source snapshots are acquired, compute case-level distributions of missing range, independent corroboration, resolution status, and provenance completeness. Reconstruct angular tracks only from verified sensor media. Keep missing and redacted telemetry distinct. The current source code and fixture do not provide observational values for statistical estimates of nonhuman technology or ER bridges.
