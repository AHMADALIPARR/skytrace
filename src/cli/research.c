// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* CLI front ends of the research ports:
 *   skytrace research <study>              print the study report (sorted-key JSON)
 *   skytrace research-check <study> <root> compare it with the committed results file
 *   skytrace port                          bounded NDJSON physical-fit protocol (one line -> one reply),
 *                                          the native replacement of research/release_22/kernel_port.py */
#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include "skytrace/research.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit(const st_json *v) {
  st_buf b; st_buf_init(&b); st_json_write(&b, v, ST_JSON_PY_CANONICAL);
  fwrite(b.data, 1, b.len, stdout); fputc('\n', stdout); st_buf_free(&b);
}

/* Keys that hold wall-clock, memory or toolchain facts of the run that produced a file; they are
 * re-measured natively and never compared. */
static const char *const SKIP[] = {
  "representation_s_all_splits", "training_tuning_s", "inference_s_per_track", "total_s",
  "per_track_full_pipeline_median_ms", "per_track_full_pipeline_p95_ms",
  "per_track_mean_ms", "total_inference_s", "machine", "construction_ms_all_splits", "training_tuning_ms",
  "p95_inference_ms", "total_measured_ms", "construction_all_s", "training_s", "wall_s",
  "peak_python_allocation_bytes", "construction_s", "prediction_s", "environment", "wall_seconds",
  "peak_process_rss_kib", "nfev"};
#define NSKIP (sizeof SKIP / sizeof *SKIP)

/* loose_keys: top-level subtrees compared with loose_atol instead (release 2.2 physical fits: the
 * Levenberg-Marquardt solver of src/fusion versus SciPy's trust-region-reflective run). */
typedef struct { const char *name; const char *file; double atol, rtol; const char *loose_keys[2]; double loose_atol; } study;
/* Tolerances (absolute + relative) against the committed Python results; see research/PORTING.md. */
static const study STUDIES[] = {
  {"parameter_recovery", "research/parameter_recovery/results.synthetic.json", 1e-9, 1e-7, {NULL, NULL}, 0},
  {"robustness", "research/parameter_recovery/robustness.synthetic.json", 1e-9, 1e-7, {NULL, NULL}, 0},
  {"graph_budget", "research/graph_budget/results.synthetic.json", 1e-9, 1e-7, {NULL, NULL}, 0},
  {"task_information", "research/task_information/results.synthetic.json", 0, 0, {NULL, NULL}, 0},
  {"source_fusion", "research/source_fusion/results.synthetic.json", 1e-9, 1e-7, {NULL, NULL}, 0},
  {"joint_fusion", "research/joint_fusion/results.synthetic.json", 1e-9, 1e-7, {NULL, NULL}, 0},
  {"numerical", "research/numerical/results.synthetic.json", 0, 0, {NULL, NULL}, 0},
  {"release22", "research/release_22/results.synthetic.json", 1e-9, 1e-7, {"physical_fit", "physical_aggregate"}, 1e-6},
};

static st_code run(const char *name, const char *root, st_json **out, st_error *err) {
  if (!strcmp(name, "parameter_recovery")) return st_research_parameter_recovery(out, err);
  if (!strcmp(name, "robustness")) return st_research_robustness(out, err);
  if (!strcmp(name, "graph_budget")) return st_research_graph_budget(out, err);
  if (!strcmp(name, "task_information")) return st_research_task_information(out, err);
  if (!strcmp(name, "numerical")) return st_research_numerical(root, out, err);
  if (!strcmp(name, "source_fusion")) return st_research_source_fusion(root, out, err);
  if (!strcmp(name, "joint_fusion")) return st_research_joint_fusion(20261117u, out, err);
  if (!strcmp(name, "release22")) return st_research_release22(root, out, err);
  return st_fail(err, ST_E_ARGUMENT, -1, "unknown study %s", name);
}

st_code cli_research(const char *name, const char *root, st_error *err) {
  st_json *o = NULL; st_code rc = run(name, root, &o, err);
  if (rc) return rc;
  emit(o); st_json_free(o);
  return ST_E_OK;
}

st_code cli_research_check(const char *name, const char *root, st_error *err) {
  const study *s = NULL;
  for (size_t i = 0; i < sizeof STUDIES / sizeof *STUDIES; ++i) if (!strcmp(STUDIES[i].name, name)) s = &STUDIES[i];
  if (!s) return st_fail(err, ST_E_ARGUMENT, -1, "unknown study %s", name);
  char path[4096]; snprintf(path, sizeof path, "%s/%s", root, s->file);
  st_json *expected = st_json_parse_file(path, err);
  if (!expected) return err->code;
  st_json *actual = NULL; st_code rc = run(name, root, &actual, err);
  if (rc) { st_json_free(expected); return rc; }
  const char *skip_all[NSKIP + 2]; size_t nskip = NSKIP;
  memcpy(skip_all, SKIP, sizeof SKIP);
  for (int k = 0; k < 2; ++k) if (s->loose_keys[k]) skip_all[nskip++] = s->loose_keys[k];
  st_cmp_policy p = { s->atol, s->rtol, skip_all, nskip }; st_cmp_report r;
  st_json_compare(actual, expected, &p, &r);
  st_cmp_report lr; memset(&lr, 0, sizeof lr);
  for (int k = 0; k < 2; ++k) {
    if (!s->loose_keys[k]) continue;
    st_cmp_policy lp = { s->loose_atol, 0, SKIP, NSKIP }; st_cmp_report one;
    st_json_compare(st_json_get(actual, s->loose_keys[k]), st_json_get(expected, s->loose_keys[k]), &lp, &one);
    lr.numbers += one.numbers; lr.exact += one.exact; lr.skipped += one.skipped;
    if (one.mismatches && !lr.mismatches) snprintf(lr.first_mismatch, sizeof lr.first_mismatch, "/%.40s%.400s", s->loose_keys[k], one.first_mismatch);
    lr.mismatches += one.mismatches;
    if (one.max_abs_diff > lr.max_abs_diff) { lr.max_abs_diff = one.max_abs_diff; snprintf(lr.worst_path, sizeof lr.worst_path, "/%.40s%.200s", s->loose_keys[k], one.worst_path); }
  }
  st_json *o = st_json_object();
  st_json_set(o, "study", st_json_string(name));
  st_json_set(o, "committed_file", st_json_string(s->file));
  st_json_set(o, "atol", st_json_double(s->atol)); st_json_set(o, "rtol", st_json_double(s->rtol));
  st_json_set(o, "numbers_compared", st_json_int((int64_t)r.numbers));
  st_json_set(o, "numbers_bit_identical", st_json_int((int64_t)r.exact));
  st_json_set(o, "keys_not_compared_timing_or_environment", st_json_int((int64_t)r.skipped));
  st_json_set(o, "max_abs_diff", st_json_double(r.max_abs_diff));
  st_json_set(o, "max_abs_diff_at", st_json_string(r.worst_path));
  st_json_set(o, "mismatches", st_json_int((int64_t)r.mismatches));
  if (r.mismatches) st_json_set(o, "first_mismatch", st_json_string(r.first_mismatch));
  if (s->loose_keys[0]) {
    st_json *l = st_json_object(), *ks = st_json_array();
    for (int k = 0; k < 2; ++k) if (s->loose_keys[k]) st_json_push(ks, st_json_string(s->loose_keys[k]));
    st_json_set(l, "keys", ks); st_json_set(l, "atol", st_json_double(s->loose_atol));
    st_json_set(l, "numbers_compared", st_json_int((int64_t)lr.numbers)); st_json_set(l, "max_abs_diff", st_json_double(lr.max_abs_diff));
    st_json_set(l, "max_abs_diff_at", st_json_string(lr.worst_path)); st_json_set(l, "mismatches", st_json_int((int64_t)lr.mismatches));
    if (lr.mismatches) st_json_set(l, "first_mismatch", st_json_string(lr.first_mismatch));
    st_json_set(o, "loose_subtrees", l);
    r.mismatches += lr.mismatches;
    if (lr.mismatches && !r.first_mismatch[0]) snprintf(r.first_mismatch, sizeof r.first_mismatch, "%s", lr.first_mismatch);
  }
  st_json_set(o, "pass", st_json_bool(r.mismatches == 0));
  emit(o);
  st_json_free(o); st_json_free(actual); st_json_free(expected);
  if (r.mismatches) return st_fail(err, ST_E_TOLERANCE, -1, "%s disagrees with %s: %s", name, s->file, r.first_mismatch);
  return ST_E_OK;
}

/* ---------------------------------------------------------------- NDJSON physical-fit port */
static st_json *fit(const st_r22_fixture *fx, int known, st_error *err) {
  st_r22_fit f;
  if (st_r22_fit_physics(fx, known, &f, err)) return NULL;
  st_json *m = st_json_object();
  st_json_set(m, "omega_error", st_json_double(f.omega_error));
  st_json_set(m, "clock_mae_s", st_json_double(f.clock_mae_s));
  st_json_set(m, "trajectory_rmse_m", st_json_double(f.trajectory_rmse_m));
  st_json_set(m, "converged", st_json_bool(f.converged));
  st_json_set(m, "nfev", st_json_int(f.nfev));
  return m;
}
static st_json *error_reply(const char *code) {
  st_json *o = st_json_object(); st_json_set(o, "status", st_json_string("error")); st_json_set(o, "code", st_json_string(code)); return o;
}
static st_json *handle(const st_json *req) {
  const st_json *op = st_json_get(req, "operation");
  if (!req || req->type != ST_JOBJ || !op || op->type != ST_JSTR || strcmp(op->str, "physical_fit")) return error_reply("E_OPERATION");
  const st_json *seed = st_json_get(req, "seed");
  if (!seed || seed->type != ST_JNUM || !seed->is_int || seed->ival < 0 || seed->ival > 1000000) return error_reply("E_SEED");
  st_r22_fixture fx; st_r22_physical_fixture((uint64_t)seed->ival, &fx);
  st_error e; st_error_clear(&e);
  st_json *k = fit(&fx, 1, &e), *u = k ? fit(&fx, 0, &e) : NULL;
  if (!k || !u) { st_json_free(k); st_json_free(u); return error_reply("E_KERNEL"); }
  st_json *o = st_json_object(); const st_json *job = st_json_get(req, "job_id");
  st_json_set(o, "status", st_json_string("ok"));
  st_json_set(o, "job_id", job ? st_json_clone(job) : st_json_null());
  st_json_set(o, "known", k); st_json_set(o, "unknown", u);
  return o;
}
st_code cli_port(st_error *err) {
  (void)err;
  const size_t MAX_LINE = 65536;
  char *line = NULL; size_t cap = 0; ssize_t n;
  while ((n = getline(&line, &cap, stdin)) >= 0) {
    if ((size_t)n > MAX_LINE) { fputs("{\"status\": \"error\", \"code\": \"E_LINE_SIZE\"}\n", stdout); fflush(stdout); continue; }
    st_error e; st_error_clear(&e);
    st_json *req = st_json_parse(line, (size_t)n, &e), *rep = req ? handle(req) : error_reply("E_REQUEST");
    emit(rep); fflush(stdout);
    st_json_free(req); st_json_free(rep);
  }
  free(line);
  return ST_E_OK;
}
