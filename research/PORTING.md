<!-- SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0 -->
<!-- Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab -->
# Research code: Python → C11 / BQN port

The research benchmarks were ported from Python to C11 (numerics) and BQN (array checks). The committed
`results*.json` files are the frozen output of the Python code and are the reference the ports are checked against.

## Mapping

| Python (removed) | C11 / BQN | Test |
|---|---|---|
| `parameter_recovery/benchmark.py` | `src/research/param_recovery.c`, `studies_pr.c`, `ridge.c`, `npcompat.c`; `skytrace research parameter_recovery` / `robustness` | `native_research_pr`, `research_parameter_recovery_vs_committed`, `research_robustness_vs_committed` |
| `parameter_recovery/tests/test_benchmark.py` | `tests/native/test_research_pr.c` | `native_research_pr` |
| `graph_budget/benchmark.py`, `test_benchmark.py` | `src/research/studies_pr.c`; `skytrace research graph_budget` | `native_research_pr`, `research_graph_budget_vs_committed` |
| `source_fusion/fusion.py`, `test_fusion.py` | `src/research/studies_pr.c`; `skytrace research source_fusion` | `native_research_pr`, `research_source_fusion_vs_committed` |
| `task_information/benchmark.py`, tests | `src/research/task_information.c`; `skytrace research task_information` | `native_research_ti_sm`, `research_task_information_vs_committed` |
| `joint_fusion/benchmark.py`, `test_benchmark.py` | `src/research/joint_fusion.c`; `skytrace research joint_fusion` | `native_research_jf`, `research_joint_fusion_vs_committed` |
| `release_22/benchmark.py` (incl. `braid_control`), `test_release.py` | `src/research/joint_fusion.c` (`st_r22_*`); `skytrace research release22` | `native_research_jf`, `research_release22_vs_committed` |
| `release_22/kernel_port.py` | `skytrace port` (NDJSON on stdin, same error codes) | `native_research_jf` (port protocol test) |
| `speculative_math/core.py`, tests | `src/research/speculative_math.c` | `native_research_ti_sm` |
| `numerical/analyze.py`, tests | `src/research/numerical.c`; `skytrace research numerical` | `native_research_ti_sm`, `research_numerical_vs_committed` |
| NumPy `Generator(PCG64)`, CPython `random` | `src/core/rng.c`, `rng_tables.c` (bit-exact) | `native_research_rng` against `fixtures/reference/rng_streams.json` |
| feature maps, ridge, quantile, triangulation, Fibonacci features | `bqn/research.bqn` | `bqn_research` (recomputes `skytrace research-export` buffers) |

SciPy/scikit-learn pieces are written out: ridge regression (standardised columns, unpenalised intercept,
LU solve, alpha chosen by first minimum validation MSE), the 'higher' quantile, `np.unwrap`, `np.interp`,
pairwise summation as in `np.sum`, `lstsq` (Householder QR), and bounded Levenberg–Marquardt in place of
SciPy's trust-region-reflective `least_squares`.

## Tolerances (`skytrace research-check <study>`)

| Study | Tolerance | Measured (numbers compared / bit-identical, max abs diff) |
|---|---|---|
| parameter_recovery | 1e-9 abs + 1e-7 rel | 45 / 24, 2.42e-11 |
| robustness | 1e-9 abs + 1e-7 rel | 60 / 0, 2.16e-13 |
| graph_budget | 1e-9 abs + 1e-7 rel | 49 / 26, 2.42e-11 |
| source_fusion | 1e-9 abs + 1e-7 rel | 43 / 22, 5.55e-16 |
| joint_fusion | 1e-9 abs + 1e-7 rel | 35 / 21, 1.93e-15 |
| task_information | exact | 8 / 8, 0 |
| numerical | exact | 45 / 45, 0 |
| release22 | 1e-9 abs + 1e-7 rel; `physical_fit`, `physical_aggregate` 1e-6 abs | 100 / 37, 4.16e-15; physical subtrees 92 numbers, 1.55e-8 |

Not compared, re-measured natively instead: wall-clock timings, memory, machine/environment fields, and
`nfev` (LM and TRF take different numbers of evaluations). The release 2.2 physical fits differ by up to
1.55e-8 m because the solver differs (LM here, SciPy TRF in the reference); both converge to the same minimum.
The physical fixture regenerated from its seeds matches `fixtures/reference/physical_fixture.seeds0-11.json` in
2827 of 2880 values bit for bit; the rest differ by ≤ 4.44e-16 (libm `atan2`/`hypot` versus NumPy).
