# BEAM source snapshot supervisor

Requires Elixir 1.14+ and **Erlang/OTP 27+** (for built-in `:json`). `cd beam && mix test` starts `Skytrace.TaskSupervisor`. Use `Skytrace.Ingest.fanout([{"pursue", "path/to/pursue.ndjson"}, {"nara", "path/to/nara.ndjson"}, {"aaro", "path/to/aaro.ndjson"}])`. All source snapshots are explicit and auditable; no unapproved live network ingestion. The `normalize_port(records, absolute_binary_path)` function sends one full batch to the OCaml normalizer. `Task.Supervisor` isolates task crashes; `async_stream_nolink` bounds concurrency and returns source-ordered results.

For large archives, split into complete forest partitions; don't divide children from their parents or unique ancestry cannot be checked. Pipelined archive records are not assumed independent eyewitness observations.
