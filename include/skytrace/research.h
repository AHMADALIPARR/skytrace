// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_RESEARCH_H
#define SKYTRACE_RESEARCH_H
/* C11 ports of the SKYTRACE 1.8-2.2 synthetic research benchmarks (formerly the Python modules under research/).
 *
 *   parameter_recovery (1.8)  constant-turn tracks, five feature branches, ridge, direct fit
 *   graph_budget       (1.9)  full attributed 18-node graph vector, per-track latency cap
 *   task_information          MT19937 angular tracks, fixed-rule sign classifiers
 *   source_fusion      (2.0)  synthetic source-registry metadata features
 *   joint_fusion       (2.1)  three-sensor bearing/range triangulation features
 *   release_22         (2.2)  frozen 2.1 check, five seeds, Fibonacci-matrix braid control
 *   speculative_math          DAG unfolding, parity code, probe replication, bridge edges
 *
 * Synthetic data are regenerated with bit-exact NumPy/CPython random streams (skytrace/rng.h), so no
 * generated data are shipped. Records are kept as st_json values with the same keys the Python
 * dictionaries had, and duplicate removal hashes the same canonical JSON with SHA-256.
 * Numerical primitives follow NumPy's definitions (pairwise summation, np.unwrap, np.interp,
 * np.quantile(method='higher'), np.linspace, np.sinc); BLAS/LAPACK products are replaced by explicit
 * sequential loops, LU with partial pivoting and Householder QR, so results agree with the Python
 * reference to rounding (tolerances: research/PORTING.md). Nothing here is evidence about UAPs. */
#include <stddef.h>
#include <stdint.h>
#include "skytrace/core.h"
#include "skytrace/rng.h"

/* ---------------------------------------------------------------- NumPy-compatible kernels */
double st_np_sum(const double *x, size_t n, size_t stride);   /* pairwise summation (np.add.reduce) */
double st_np_mean(const double *x, size_t n, size_t stride);
double st_np_std(const double *x, size_t n, size_t stride);   /* ddof = 0 */
double st_np_median(const double *x, size_t n);
double st_np_quantile_higher(const double *x, size_t n, double q);
double st_np_mod(double a, double b);                          /* np.mod / np.remainder */
void st_np_unwrap(const double *p, size_t n, double *out);     /* period 2 pi, discont pi */
double st_np_interp(double x, const double *xp, const double *fp, size_t n);
void st_np_linspace(double a, double b, size_t n, double *out);
double st_np_sinc(double x);
double st_np_sign(double x);
double st_polyfit_slope(const double *x, const double *y, size_t n); /* degree-1 least-squares slope */
/* min ||A x - b|| for A (m x 2, row-major, m >= 2) by Householder QR; rank by SVD tolerance
 * smax * max(m, 2) * DBL_EPSILON (np.linalg.matrix_rank). Rank-deficient systems fail E_SINGULAR. */
st_code st_lstsq2(const double *A, const double *b, size_t m, double x[2], st_error *err);
int st_matrix_rank2(const double *A, size_t m);

/* ---------------------------------------------------------------- ridge regression */
#define ST_RIDGE_MAX_P 64
typedef struct { double alpha; size_t p; double mean[ST_RIDGE_MAX_P], sd[ST_RIDGE_MAX_P], beta[ST_RIDGE_MAX_P + 1]; } st_ridge;
extern const double ST_BUDGET_ALPHAS[5];   /* 0.01, 0.1, 1, 10, 100 */
/* Standardise columns (population sd, sd < 1e-9 -> 1), unpenalised intercept, solve the normal
 * equations (A^T A + alpha I') beta = A^T y with LU and partial pivoting. X is n x p row-major. */
st_code st_ridge_fit(const double *X, const double *y, size_t n, size_t p, double alpha, st_ridge *m, st_error *err);
double st_ridge_predict1(const st_ridge *m, const double *x);
/* Fits all five budget alphas on (Xtr, ytr) and keeps the first minimum of validation MSE. */
st_code st_ridge_select(const double *Xtr, const double *ytr, size_t ntr, const double *Xv, const double *yv, size_t nv,
                        size_t p, st_ridge *best, st_error *err);
typedef struct { double mae, rmse, bias; } st_reg_metrics;
st_reg_metrics st_reg_metrics_of(const double *truth, const double *pred, size_t n);

/* ---------------------------------------------------------------- time */
double st_now_s(void);   /* CLOCK_MONOTONIC seconds */

/* ---------------------------------------------------------------- 1.8 parameter recovery */
#define ST_PR_SEED 20261117u
typedef struct { int id; double theta_rad_s, speed_m_s; st_json *records; /* array of record objects */ } st_pr_row;
typedef struct { st_pr_row *rows; size_t n; size_t *order; size_t ntrain, nval, ntest; } st_pr_data;
/* simulate(index, rng, n, sigma, jitter, drop) */
st_code st_pr_simulate(int index, st_np_rng *g, size_t n, double sigma, double jitter, double drop, st_pr_row *row, st_error *err);
st_code st_pr_generate(st_pr_data *d, st_error *err);   /* 360 tracks, seed 20261117, split 220/70/70 */
void st_pr_free(st_pr_data *d);
void st_pr_row_free(st_pr_row *r);
const st_pr_row *st_pr_split(const st_pr_data *d, int which, size_t i); /* which: 0 train, 1 val, 2 test */
size_t st_pr_split_n(const st_pr_data *d, int which);
st_code st_pr_record_digest(const st_json *rec, char hex[65], st_error *err);
/* canonicalize: unique by digest, sorted by digest; returns a new array the caller frees. */
st_code st_pr_canonicalize(const st_json *records, st_json **out, st_error *err);

typedef enum { ST_PR_GRAPH = 0, ST_PR_TEMPORAL, ST_PR_BRAID_WORD, ST_PR_BRAID_TIMED, ST_PR_BRAID_METRIC, ST_PR_FULL_GRAPH } st_pr_branch;
#define ST_PR_MAX_FEATURES 64
/* Feature vector of a record list; returns the count in *nf. E_TIME / E_SCHEMA on invalid tracks. */
st_code st_pr_features(const st_json *records, st_pr_branch b, double *f, size_t *nf, st_error *err);
st_code st_pr_braid_word(const st_json *records, int *word, size_t *n, size_t cap, st_error *err);
/* Direct constant-turn least squares on positions (bounded Levenberg-Marquardt, analytic Jacobian):
 * returns omega. Bounds v in [0.1, 30], omega in [-0.5, 0.5]. */
st_code st_pr_direct_fit(const st_json *records, double *omega, st_error *err);
typedef struct { int same_braid_word, same_temporal_features; } st_pr_collision;
st_code st_pr_collision_example(st_pr_collision *c, st_error *err);

/* Reports with the keys of the former Python JSON outputs. Timing fields are measured natively. */
st_code st_research_parameter_recovery(st_json **out, st_error *err);
st_code st_research_robustness(st_json **out, st_error *err);
st_code st_research_graph_budget(st_json **out, st_error *err);
st_code st_research_source_fusion(const char *root, st_json **out, st_error *err);
st_code st_research_task_information(st_json **out, st_error *err);
st_code st_research_joint_fusion(uint64_t seed, st_json **out, st_error *err);
st_code st_research_release22(const char *root, st_json **out, st_error *err);
st_code st_research_numerical(const char *root, st_json **out, st_error *err);

/* ---------------------------------------------------------------- numerical closure audit (research/numerical) */
int st_num_kernel_active(int range, int corroborated, int resolution);   /* ternary codes -1 unknown, 0, 1 */
int st_num_strict_active(int range, int corroborated, int resolution);
st_code st_num_metrics(const st_json *records, st_json **out, st_error *err); /* graph + closure counts */
st_json *st_num_truth_table(void);
st_code st_num_braid_audit(st_json **out, st_error *err);

/* ---------------------------------------------------------------- source fusion */
typedef struct { char source_id[16]; char source_url[256]; char source_tier[32]; char record_id[32]; char origin[48];
                 const char *range_telemetry, *corroboration, *resolution, *link_status; char calibration_id[64]; double sigma_m; } st_sf_metadata;
typedef struct { st_json *registry; } st_sf_registry;
st_code st_sf_load(const char *root, st_sf_registry *r, st_error *err);
void st_sf_free(st_sf_registry *r);
st_code st_sf_attached_metadata(const st_sf_registry *r, const st_pr_row *row, st_sf_metadata *m, st_error *err);
st_code st_sf_validate(const st_sf_registry *r, const st_sf_metadata *m, st_error *err);
typedef enum { ST_SF_TEMPORAL = 0, ST_SF_TEMPORAL_SOURCE, ST_SF_TEMPORAL_BRAID, ST_SF_TEMPORAL_SOURCE_BRAID, ST_SF_SOURCE_ONLY } st_sf_branch;
st_code st_sf_features(const st_sf_registry *r, const st_pr_row *row, st_sf_branch b, double *f, size_t *nf, st_error *err);

/* ---------------------------------------------------------------- task information */
typedef struct { int label; st_json *records; } st_ti_row;
st_code st_ti_generate(size_t n, uint64_t seed, st_ti_row **rows, st_error *err);
void st_ti_free(st_ti_row *rows, size_t n);
typedef struct { size_t count; char fingerprint[65]; double agnostic[2], temporal, braid[3]; size_t nword; int word[64]; } st_ti_repr;
st_code st_ti_canonical_origins(const st_json *records, st_json **out, st_error *err);
st_code st_ti_representation(const st_json *records, st_ti_repr *r, st_error *err);

/* ---------------------------------------------------------------- joint fusion (2.1) */
typedef struct { int id; double omega_rad_s; st_json *records; } st_jf_row;
st_code st_jf_simulate(int i, st_np_rng *g, st_jf_row *row, st_error *err);
void st_jf_row_free(st_jf_row *r);
st_code st_jf_canon(const st_json *records, st_json **out, st_error *err);
st_code st_jf_validate(const st_json *records, st_json **out, st_error *err);
void st_jf_xy_path(double t, double x0, double y0, double v, double psi, double w, double *x, double *y);
#define ST_JF_GRID 12
st_code st_jf_points(const st_json *records, int use_range, int use_multisensor, double grid[ST_JF_GRID],
                     double xy[ST_JF_GRID][2], int *rank, st_error *err);
typedef enum { ST_JF_BEARING_ONLY = 0, ST_JF_MULTIBEARING, ST_JF_FUSED, ST_JF_FUSED_BRAID } st_jf_branch;
st_code st_jf_features(const st_json *records, st_jf_branch b, double *f, size_t *nf, st_error *err);
/* Release 2.2: Fibonacci representation of the fused angular-rate word, Re then Im, row-major. */
st_code st_r22_braid_matrix_features(const st_json *records, double f[8], st_error *err);
/* Release 2.2 physical fixture regenerated from its seed (rows: sensor, reported time, bearing, range). */
typedef struct { double truth[5], offsets[3]; double rows[60][4]; } st_r22_fixture;
void st_r22_physical_fixture(uint64_t seed, st_r22_fixture *f);
/* fit_physics: known = 1 subtracts the injected offsets (reference comparison only); known = 0
 * estimates delta_1, delta_2 with sensor 0 as the reference clock. Solver: src/fusion (LM). */
typedef struct { double omega_error, clock_mae_s, trajectory_rmse_m; int converged, nfev; } st_r22_fit;
st_code st_r22_fit_physics(const st_r22_fixture *fx, int known, st_r22_fit *out, st_error *err);

/* ---------------------------------------------------------------- speculative math */
#define ST_SM_MAX_NODES 64
/* Deterministic Kahn topological order (smallest available name first); duplicate nodes, duplicate
 * edges, dangling edges and cycles are rejected (E_DUPLICATE, E_DANGLING, E_CYCLE). */
st_code st_sm_validate_dag(const char *const *nodes, size_t nn, const char *const (*edges)[2], size_t ne,
                           size_t *order, st_error *err);
/* Root-to-leaf paths sorted lexicographically by node names, written back to back as node-index lists
 * each terminated by (size_t)-1. More than max_paths pending+finished paths fails with E_OVERFLOW. */
st_code st_sm_unfold_paths(const char *const *nodes, size_t nn, const char *const (*edges)[2], size_t ne,
                           const char *root, size_t max_paths, size_t *paths, size_t cap, size_t *npaths, st_error *err);
st_code st_sm_parity_encode(const int *bits, size_t n, int *out, st_error *err); /* out has n + 1 slots */
st_code st_sm_parity_syndrome(const int *code, size_t n, int *syndrome, st_error *err);
typedef struct { long generation, count, resource_units; } st_sm_probe;
st_code st_sm_replicate_probes(long initial, long resources, long cost_per_child, long branching, long generations,
                               st_sm_probe *states, size_t cap, size_t *n, st_error *err);
/* Graph-layer scenario only: returns {status: speculative_not_observed, vertices (sorted), edges (sorted)}. */
st_code st_sm_bridge_edges(const char *const *locations, size_t nl, const char *const (*bridges)[2], size_t nb, st_json **out, st_error *err);

/* ---------------------------------------------------------------- committed-result comparison */
typedef struct { double atol, rtol; const char *const *skip_keys; size_t nskip; } st_cmp_policy;
typedef struct { size_t numbers, exact, skipped, mismatches; double max_abs_diff; char worst_path[256]; char first_mismatch[512]; } st_cmp_report;
/* Every key of `expected` must be present in `actual`; numbers within atol + rtol |expected|
 * (integers and booleans exactly), strings exactly. Keys named in skip_keys are not compared. */
void st_json_compare(const st_json *actual, const st_json *expected, const st_cmp_policy *p, st_cmp_report *r);

#endif
