# OPAM / OCaml normalizer

Dependencies: OCaml 5.x, `opam install dune yojson`. Build `opam exec -- dune build`, run `opam exec -- dune exec skytrace-normalize`. The binary accepts **one JSON request per line**, outputs exactly one JSON response per request; `{ "records": [ ... ] }` requests ensure parent containment is checked as a *batch*. Required fields: `source`, `agency`, `canonical_id`, `release_epoch`, `mime`. Optional: `parent_id`, `has_range_telemetry`, `corroborated`, `resolved`. Missing booleans map to the explicit `unknown` state, not false. Parent IDs must resolve within the batch; build complete hierarchy snapshots, not orphan fragments.

The composite key is a readable unit-separator tuple; the storage key must be the SHA-256 of canonical field encodings in the upstream evidence layer. This binary is not the raw artifact store. `Angstrom` is deliberately not a parser dependency: untrusted HTML must be converted by a source-specific adapter into structured JSON first, rather than attempting to parse arbitrary publisher HTML with a partial grammar.

Adapter input examples and validation tests are in `fixtures/archives/synthetic-batch.json`. The BEAM `Skytrace.Adapters` module maps published structured row fields for PURSUE, NARA, and AARO; schema drift fails closed. Unknown data is not inferred from a filename.
