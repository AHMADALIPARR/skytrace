# SAW gate (C/LLVM)

Run `make -C kernel test bitcode` and `saw proofs/saw/closure.saw` at repo root.
The script targets `st_incomplete` and `st_active` in C-generated LLVM bitcode and proves their bitvector functional specifications with Z3. **This does not prove `st_validate`'s arbitrary-size forest invariant:** the graph/unique-parent checks are native tested and require a separate bounded or inductive proof if marked formally proven. SAW was not available in the packaging environment; do not mark G7 passed without its proof log.

A distinct model-level statement is required: an acyclic DAG may have *multiple* root-to-leaf paths. SKYTRACE therefore uses a rooted forest for `contains` and an independent acyclic graph for `derived_from` provenance relations.
