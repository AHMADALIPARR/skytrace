// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Reports of the 1.8 parameter-recovery benchmark and robustness study, the 1.9 graph-budget
 * study and the 2.0 source-fusion ablation, with the same keys as the former Python outputs.
 * Wall-clock fields are measured here, natively, and are not comparable with the Python ones. */
#define _POSIX_C_SOURCE 200809L
#include "skytrace/research.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

typedef st_code (*extract_fn)(const void *ctx, const st_pr_row *row, double *f, size_t *nf, st_error *err);
typedef struct { st_pr_branch b; } pr_ctx;
static st_code pr_extract(const void *ctx, const st_pr_row *row, double *f, size_t *nf, st_error *err) {
  return st_pr_features(row->records, ((const pr_ctx *)ctx)->b, f, nf, err);
}

/* Feature matrix of one split. */
static st_code matrix(const st_pr_data *d, int w, extract_fn fn, const void *ctx, double **X, double **y, size_t *p, st_error *err) {
  size_t n = st_pr_split_n(d, w);
  *X = malloc(n * ST_PR_MAX_FEATURES * sizeof **X); *y = malloc(n * sizeof **y);
  if (!*X || !*y) return st_fail(err, ST_E_NOMEM, -1, "matrix");
  double f[ST_PR_MAX_FEATURES];
  for (size_t i = 0; i < n; ++i) {
    size_t nf = 0; const st_pr_row *r = st_pr_split(d, w, i);
    st_code rc = fn(ctx, r, f, &nf, err);
    if (rc) return rc;
    if (i == 0) *p = nf;
    memcpy(*X + i * nf, f, nf * sizeof *f);
    (*y)[i] = r->theta_rad_s;
  }
  return ST_E_OK;
}

typedef struct { st_ridge model; double q; st_reg_metrics m; double coverage, width; size_t p;
                 double representation_s, tuning_s, inference_s_per_track; double *pred; size_t nt; } branch_eval;

static st_code evaluate_branch(const st_pr_data *d, extract_fn fn, const void *ctx, branch_eval *o, st_error *err) {
  memset(o, 0, sizeof *o);
  double *Xtr = NULL, *ytr = NULL, *Xv = NULL, *yv = NULL, *Xt = NULL, *yt = NULL; size_t p = 0;
  double tick = st_now_s();
  st_code rc = matrix(d, 0, fn, ctx, &Xtr, &ytr, &p, err);
  if (!rc) rc = matrix(d, 1, fn, ctx, &Xv, &yv, &p, err);
  if (!rc) rc = matrix(d, 2, fn, ctx, &Xt, &yt, &p, err);
  o->representation_s = st_now_s() - tick;
  size_t nv = d->nval, nt = d->ntest;
  if (!rc) {
    tick = st_now_s();
    rc = st_ridge_select(Xtr, ytr, d->ntrain, Xv, yv, nv, p, &o->model, err);
  }
  if (!rc) {
    double *ev = malloc(nv * sizeof *ev), *cov = malloc(nt * sizeof *cov);
    o->pred = malloc(nt * sizeof *o->pred); o->nt = nt;
    for (size_t i = 0; i < nv; ++i) ev[i] = fabs(st_ridge_predict1(&o->model, Xv + i * p) - yv[i]);
    for (size_t i = 0; i < nt; ++i) o->pred[i] = st_ridge_predict1(&o->model, Xt + i * p);
    o->tuning_s = st_now_s() - tick;
    o->q = st_np_quantile_higher(ev, nv, .9);
    o->m = st_reg_metrics_of(yt, o->pred, nt);
    for (size_t i = 0; i < nt; ++i) cov[i] = fabs(o->pred[i] - yt[i]) <= o->q ? 1. : 0.;
    o->coverage = st_np_mean(cov, nt, 1); o->width = 2 * o->q; o->p = p;
    tick = st_now_s();
    volatile double sink = 0;
    for (size_t i = 0; i < nt; ++i) sink += st_ridge_predict1(&o->model, Xt + i * p);
    (void)sink;
    o->inference_s_per_track = (st_now_s() - tick) / (double)nt;
    free(ev); free(cov);
  }
  free(Xtr); free(ytr); free(Xv); free(yv); free(Xt); free(yt);
  return rc;
}

static void put_metrics(st_json *o, st_reg_metrics m) {
  st_json_set(o, "mae_rad_s", st_json_double(m.mae));
  st_json_set(o, "rmse_rad_s", st_json_double(m.rmse));
  st_json_set(o, "bias_rad_s", st_json_double(m.bias));
}

static const char *PR_NAMES[5] = {"graph_summary", "temporal", "braid_word", "braid_timed", "braid_metric"};
static const st_pr_branch PR_BRANCHES[5] = {ST_PR_GRAPH, ST_PR_TEMPORAL, ST_PR_BRAID_WORD, ST_PR_BRAID_TIMED, ST_PR_BRAID_METRIC};

static st_code features_equal(const st_json *a, const st_json *b, st_pr_branch br, int *eq, st_error *err) {
  double f1[ST_PR_MAX_FEATURES], f2[ST_PR_MAX_FEATURES]; size_t n1, n2;
  st_code rc = st_pr_features(a, br, f1, &n1, err);
  if (!rc) rc = st_pr_features(b, br, f2, &n2, err);
  if (rc) return rc;
  *eq = n1 == n2;
  for (size_t i = 0; *eq && i < n1; ++i) *eq = f1[i] == f2[i];
  return ST_E_OK;
}
static st_json *duplicate_of_first(const st_json *records) {
  st_json *dup = st_json_clone(records);
  st_json_push(dup, st_json_clone(records->items[0]));
  return dup;
}
static st_json *collision_json(st_error *err, st_code *rc) {
  st_pr_collision c; *rc = st_pr_collision_example(&c, err);
  st_json *o = st_json_object();
  st_json_set(o, "omega_a_rad_s", st_json_double(.04));
  st_json_set(o, "omega_b_rad_s", st_json_double(.12));
  st_json_set(o, "same_braid_word", st_json_bool(c.same_braid_word));
  st_json_set(o, "same_temporal_features", st_json_bool(c.same_temporal_features));
  return o;
}

static st_code direct_reference(const st_pr_data *d, double *pred, double *truth, double *elapsed, st_error *err) {
  double tick = st_now_s();
  for (size_t i = 0; i < d->ntest; ++i) {
    const st_pr_row *r = st_pr_split(d, 2, i);
    st_code rc = st_pr_direct_fit(r->records, &pred[i], err);
    if (rc) return rc;
    truth[i] = r->theta_rad_s;
  }
  *elapsed = st_now_s() - tick;
  return ST_E_OK;
}

st_code st_research_parameter_recovery(st_json **out, st_error *err) {
  st_pr_data d; st_code rc = st_pr_generate(&d, err);
  if (rc) { st_pr_free(&d); return rc; }
  st_json *o = st_json_object(), *branches = st_json_object();
  for (int k = 0; k < 5 && !rc; ++k) {
    pr_ctx c = {PR_BRANCHES[k]}; branch_eval e = {0};
    rc = evaluate_branch(&d, pr_extract, &c, &e, err);
    if (rc) break;
    st_json *b = st_json_object();
    put_metrics(b, e.m);
    st_json_set(b, "alpha", st_json_double(e.model.alpha));
    st_json_set(b, "interval_90_coverage", st_json_double(e.coverage));
    st_json_set(b, "interval_90_width_rad_s", st_json_double(e.width));
    st_json_set(b, "representation_s_all_splits", st_json_double(e.representation_s));
    st_json_set(b, "training_tuning_s", st_json_double(e.tuning_s));
    st_json_set(b, "inference_s_per_track", st_json_double(e.inference_s_per_track));
    st_json_set(b, "feature_count", st_json_int((int64_t)e.p));
    st_json_set(b, "total_s", st_json_double(e.representation_s + e.tuning_s + e.inference_s_per_track * (double)d.ntest));
    st_json_set(branches, PR_NAMES[k], b);
    free(e.pred);
  }
  double pred[70], truth[70], elapsed = 0;
  if (!rc) rc = direct_reference(&d, pred, truth, &elapsed, err);
  st_json *direct = st_json_object();
  if (!rc) { put_metrics(direct, st_reg_metrics_of(truth, pred, d.ntest)); st_json_set(direct, "inference_s_per_track", st_json_double(elapsed / (double)d.ntest)); }
  st_json *inv = st_json_object();
  if (!rc) {
    const st_json *orig = st_pr_split(&d, 2, 0)->records; st_json *dup = duplicate_of_first(orig);
    for (int k = 0; k < 5 && !rc; ++k) { int eq = 0; rc = features_equal(orig, dup, PR_BRANCHES[k], &eq, err); st_json_set(inv, PR_NAMES[k], st_json_bool(eq)); }
    st_json_free(dup);
  }
  st_json *coll = NULL; if (!rc) coll = collision_json(err, &rc);
  st_json_set(o, "experiment", st_json_string("synthetic_constant_turn_parameter_recovery"));
  st_json_set(o, "seed", st_json_int(ST_PR_SEED));
  st_json *u = st_json_object();
  st_json_set(u, "target", st_json_string("rad/s")); st_json_set(u, "coordinates", st_json_string("meters")); st_json_set(u, "time", st_json_string("seconds"));
  st_json_set(o, "units", u);
  st_json *sp = st_json_object();
  st_json_set(sp, "train", st_json_int((int64_t)d.ntrain)); st_json_set(sp, "validation", st_json_int((int64_t)d.nval));
  st_json_set(sp, "test", st_json_int((int64_t)d.ntest)); st_json_set(sp, "group", st_json_string("trajectory"));
  st_json_set(o, "split", sp);
  st_json_set(o, "noise_sigma_m", st_json_double(.035));
  st_json_set(o, "sensors", st_json_string("direct_position"));
  st_json_set(o, "branches", branches);
  st_json_set(o, "direct_model_reference", direct);
  st_json_set(o, "duplication_invariance", inv);
  st_json_set(o, "word_collision_control", coll ? coll : st_json_null());
  st_json_set(o, "coverage_method", st_json_string("split absolute-residual quantile on validation; marginal, not conditional guarantee"));
  st_json_set(o, "runtime_note", st_json_string("wall-clock measurements include feature construction, tuning and held-out prediction; exclude interpreter startup and data generation; repeat on fixed dedicated hardware for comparisons"));
  st_json_set(o, "claim_boundary", st_json_string("Synthetic model only; not a physical UAP inference or quantum-information claim."));
  st_json_set(o, "implementation", st_json_string("C11 port (src/research); timings are native measurements"));
  st_pr_free(&d);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- robustness */
st_code st_research_robustness(st_json **out, st_error *err) {
  st_pr_data d; st_code rc = st_pr_generate(&d, err);
  if (rc) { st_pr_free(&d); return rc; }
  const char *names[4] = {"temporal", "braid_word", "braid_timed", "braid_metric"};
  const st_pr_branch br[4] = {ST_PR_TEMPORAL, ST_PR_BRAID_WORD, ST_PR_BRAID_TIMED, ST_PR_BRAID_METRIC};
  st_ridge trained[4];
  for (int k = 0; k < 4 && !rc; ++k) {
    pr_ctx c = {br[k]}; double *Xtr = NULL, *ytr = NULL, *Xv = NULL, *yv = NULL; size_t p = 0;
    rc = matrix(&d, 0, pr_extract, &c, &Xtr, &ytr, &p, err);
    if (!rc) rc = matrix(&d, 1, pr_extract, &c, &Xv, &yv, &p, err);
    if (!rc) rc = st_ridge_select(Xtr, ytr, d.ntrain, Xv, yv, d.nval, p, &trained[k], err);
    free(Xtr); free(ytr); free(Xv); free(yv);
  }
  const char *sc[5] = {"base", "extra_noise_0p1m", "time_jitter_0p03s", "missing_every_fourth", "half_duration"};
  st_json *o = st_json_object();
  size_t nt = d.ntest; double yt[70];
  for (size_t i = 0; i < nt; ++i) yt[i] = st_pr_split(&d, 2, i)->theta_rad_s;
  for (int s = 0; s < 5 && !rc; ++s) {
    st_json **pert = calloc(nt, sizeof *pert);
    for (size_t i = 0; i < nt; ++i) {
      const st_pr_row *r = st_pr_split(&d, 2, i);
      st_json *copy = st_json_clone(r->records->items[0]);
      st_np_rng g; st_np_seed(&g, (uint64_t)ST_PR_SEED + (uint64_t)r->id * 17u);
      st_json *pos = st_json_get(copy, "positions_m"), *ts = st_json_get(copy, "times_s");
      size_t n = ts->count;
      double *t = malloc(n * sizeof *t), *x = malloc(n * sizeof *x), *y = malloc(n * sizeof *y);
      for (size_t k = 0; k < n; ++k) { t[k] = ts->items[k]->num; x[k] = pos->items[k]->items[0]->num; y[k] = pos->items[k]->items[1]->num; }
      size_t m = n;
      if (s == 1) for (size_t k = 0; k < n; ++k) { x[k] += st_np_normal(&g, 0, .1); y[k] += st_np_normal(&g, 0, .1); }
      else if (s == 2) {
        for (size_t k = 0; k < n; ++k) t[k] = t[k] + st_np_normal(&g, 0, .03);
        t[0] = 0.;
        for (size_t a = 1; a < n; ++a) { double v = t[a]; size_t b = a; while (b > 0 && t[b-1] > v) { t[b] = t[b-1]; --b; } t[b] = v; }
      } else if (s == 3) {
        m = 0;
        for (size_t k = 0; k < n; ++k) if (k % 4 != 0 || k == 0 || k == n - 1) { t[m] = t[k]; x[m] = x[k]; y[m] = y[k]; ++m; }
      } else if (s == 4) { m = n / 2 > 5 ? n / 2 : 5; }
      st_json *np = st_json_array(), *nts = st_json_array();
      for (size_t k = 0; k < m; ++k) {
        st_json *pt = st_json_array(); st_json_push(pt, st_json_double(x[k])); st_json_push(pt, st_json_double(y[k]));
        st_json_push(np, pt); st_json_push(nts, st_json_double(t[k]));
      }
      st_json_set(copy, "positions_m", np); st_json_set(copy, "times_s", nts);
      pert[i] = st_json_array(); st_json_push(pert[i], copy);
      free(t); free(x); free(y);
    }
    st_json *so = st_json_object();
    for (int k = 0; k < 4 && !rc; ++k) {
      double pred[70], f[ST_PR_MAX_FEATURES]; size_t nf = 0;
      for (size_t i = 0; i < nt && !rc; ++i) { rc = st_pr_features(pert[i], br[k], f, &nf, err); if (!rc) pred[i] = st_ridge_predict1(&trained[k], f); }
      if (rc) break;
      st_json *mo = st_json_object(); put_metrics(mo, st_reg_metrics_of(yt, pred, nt));
      st_json_set(so, names[k], mo);
    }
    st_json_set(o, sc[s], so);
    for (size_t i = 0; i < nt; ++i) st_json_free(pert[i]);
    free(pert);
  }
  st_pr_free(&d);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- 1.9 graph budget */
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double percentile_linear(const double *v, size_t n, double pct) {  /* np.percentile default 'linear' */
  double *c = malloc(n * sizeof *c); memcpy(c, v, n * sizeof *c); qsort(c, n, sizeof *c, cmpd);
  double h = (double)(n - 1) * pct / 100.0; size_t lo = (size_t)floor(h), hi = lo + 1 < n ? lo + 1 : lo;
  double r = c[lo] + (h - (double)lo) * (c[hi] - c[lo]); free(c); return r;
}

st_code st_research_graph_budget(st_json **out, st_error *err) {
  st_pr_data d; st_code rc = st_pr_generate(&d, err);
  if (rc) { st_pr_free(&d); return rc; }
  const char *names[5] = {"full_attributed_graph", "temporal", "braid_word", "braid_timing", "braid_metrics"};
  const st_pr_branch br[5] = {ST_PR_FULL_GRAPH, ST_PR_TEMPORAL, ST_PR_BRAID_WORD, ST_PR_BRAID_TIMED, ST_PR_BRAID_METRIC};
  st_json *o = st_json_object(), *branches = st_json_object();
  const double cap_ms = 50.;
  for (int k = 0; k < 5 && !rc; ++k) {
    pr_ctx c = {br[k]}; branch_eval e = {0};
    rc = evaluate_branch(&d, pr_extract, &c, &e, err);
    if (rc) break;
    double lat[70], f[ST_PR_MAX_FEATURES]; size_t nf = 0; double pred[70], yt[70], cov[70];
    for (size_t i = 0; i < d.ntest && !rc; ++i) {
      const st_pr_row *r = st_pr_split(&d, 2, i);
      double s = st_now_s();
      rc = st_pr_features(r->records, br[k], f, &nf, err);
      pred[i] = st_ridge_predict1(&e.model, f);
      lat[i] = (st_now_s() - s) * 1e3; yt[i] = r->theta_rad_s;
    }
    if (rc) { free(e.pred); break; }
    for (size_t i = 0; i < d.ntest; ++i) cov[i] = fabs(yt[i] - pred[i]) <= e.q ? 1. : 0.;
    st_json *b = st_json_object();
    put_metrics(b, st_reg_metrics_of(yt, pred, d.ntest));
    double p95 = percentile_linear(lat, d.ntest, 95);
    st_json_set(b, "interval_90_coverage", st_json_double(st_np_mean(cov, d.ntest, 1)));
    st_json_set(b, "interval_90_width_rad_s", st_json_double(2 * e.q));
    st_json_set(b, "alpha", st_json_double(e.model.alpha));
    st_json_set(b, "feature_count", st_json_int((int64_t)e.p));
    st_json_set(b, "representation_s_all_splits", st_json_double(e.representation_s));
    st_json_set(b, "training_tuning_s", st_json_double(e.tuning_s));
    st_json_set(b, "per_track_full_pipeline_median_ms", st_json_double(st_np_median(lat, d.ntest)));
    st_json_set(b, "per_track_full_pipeline_p95_ms", st_json_double(p95));
    st_json_set(b, "per_track_budget_cap_ms", st_json_double(cap_ms));
    st_json_set(b, "cap_pass_p95", st_json_bool(p95 <= cap_ms));
    st_json_set(branches, names[k], b);
    free(e.pred);
  }
  double pred[70], truth[70], elapsed = 0;
  if (!rc) rc = direct_reference(&d, pred, truth, &elapsed, err);
  st_json *ref = st_json_object();
  if (!rc) {
    put_metrics(ref, st_reg_metrics_of(truth, pred, d.ntest));
    st_json_set(ref, "per_track_mean_ms", st_json_double(1000 * elapsed / (double)d.ntest));
    st_json_set(ref, "total_inference_s", st_json_double(elapsed));
  }
  st_json *inv = st_json_object();
  if (!rc) {
    const st_json *orig = st_pr_split(&d, 2, 0)->records; st_json *dup = duplicate_of_first(orig);
    for (int k = 0; k < 5 && !rc; ++k) { int eq = 0; rc = features_equal(orig, dup, br[k], &eq, err); st_json_set(inv, names[k], st_json_bool(eq)); }
    st_json_free(dup);
  }
  st_json *coll = NULL; if (!rc) coll = collision_json(err, &rc);
  st_json_set(o, "version", st_json_string("SKYTRACE 1.9"));
  st_json_set(o, "dataset", st_json_string("synthetic direct-position constant-turn"));
  st_json_set(o, "seed", st_json_int(ST_PR_SEED));
  st_json *sp = st_json_object();
  st_json_set(sp, "training", st_json_int((int64_t)d.ntrain)); st_json_set(sp, "validation", st_json_int((int64_t)d.nval)); st_json_set(sp, "held_out", st_json_int((int64_t)d.ntest));
  st_json_set(o, "split", sp);
  st_json_set(o, "reference", ref);
  st_json_set(o, "branches", branches);
  st_json_set(o, "duplicate_invariance", inv);
  st_json_set(o, "collision_control", coll ? coll : st_json_null());
  st_json_set(o, "budget_rule", st_json_string("50 ms p95 single-trajectory construction plus prediction (not training); training cost reported separately; runs on shared host so no hardware-equality claim"));
  struct utsname un; st_json *mach = st_json_object();
  st_json_set(mach, "implementation", st_json_string("C11 (skytrace research)"));
  if (uname(&un) == 0) { char pl[300]; snprintf(pl, sizeof pl, "%s-%s-%s", un.sysname, un.release, un.machine); st_json_set(mach, "platform", st_json_string(pl)); }
  st_json_set(o, "machine", mach);
  st_json_set(o, "interpretation", st_json_string("Full attributed graph = fixed 18-node ordered vertex attribute vector with ridge regression; not graph neural network or exhaustive graph estimator. Full-graph vector is a higher-capacity baseline; 1.8 graph summary remains explicitly lossy."));
  st_pr_free(&d);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- 2.0 source fusion */
static const char *SOURCE_ORDER[3] = {"pursue", "aaro", "nara"};

st_code st_sf_load(const char *root, st_sf_registry *r, st_error *err) {
  char path[4096]; snprintf(path, sizeof path, "%s/data/sources.json", root);
  r->registry = st_json_parse_file(path, err);
  return r->registry ? ST_E_OK : (err ? err->code : ST_E_IO);
}
void st_sf_free(st_sf_registry *r) { st_json_free(r->registry); r->registry = NULL; }
static const st_json *source_by_id(const st_sf_registry *r, const char *id) {
  const st_json *s = st_json_get(r->registry, "sources");
  for (size_t i = 0; s && i < s->count; ++i) { const st_json *x = st_json_get(s->items[i], "id"); if (x && x->type == ST_JSTR && !strcmp(x->str, id)) return s->items[i]; }
  return NULL;
}
st_code st_sf_attached_metadata(const st_sf_registry *r, const st_pr_row *row, st_sf_metadata *m, st_error *err) {
  memset(m, 0, sizeof *m);
  const char *sid = SOURCE_ORDER[row->id % 3];
  const st_json *src = source_by_id(r, sid);
  if (!src) return st_fail(err, ST_E_SOURCE_POLICY, -1, "unknown source");
  const st_json *rec = row->records->items[0];
  snprintf(m->source_id, sizeof m->source_id, "%s", sid);
  snprintf(m->source_url, sizeof m->source_url, "%s", st_json_get(src, "url")->str);
  snprintf(m->source_tier, sizeof m->source_tier, "%s", st_json_get(src, "tier")->str);
  snprintf(m->record_id, sizeof m->record_id, "SYNTHETIC-ONLY-%04d", row->id);
  snprintf(m->origin, sizeof m->origin, "%s", st_json_get(rec, "origin")->str);
  m->link_status = "synthetic_association"; m->range_telemetry = "unknown"; m->corroboration = "unknown"; m->resolution = "unknown";
  snprintf(m->calibration_id, sizeof m->calibration_id, "%s", st_json_get(rec, "calibration")->str);
  m->sigma_m = st_json_get(rec, "sigma_m")->num;
  return ST_E_OK;
}
st_code st_sf_validate(const st_sf_registry *r, const st_sf_metadata *m, st_error *err) {
  const st_json *src = source_by_id(r, m->source_id);
  if (!src) return st_fail(err, ST_E_SOURCE_POLICY, -1, "unknown source");
  if (strcmp(m->source_url, st_json_get(src, "url")->str)) return st_fail(err, ST_E_SOURCE_POLICY, -1, "source URL mismatch");
  if (strcmp(m->link_status, "synthetic_association")) return st_fail(err, ST_E_SOURCE_POLICY, -1, "unexpected source binding");
  if (strcmp(m->range_telemetry, "unknown") && strcmp(m->range_telemetry, "present") && strcmp(m->range_telemetry, "absent")) return st_fail(err, ST_E_SCHEMA, -1, "invalid range state");
  if (strcmp(m->corroboration, "unknown") && strcmp(m->corroboration, "yes") && strcmp(m->corroboration, "no")) return st_fail(err, ST_E_SCHEMA, -1, "invalid corroboration");
  return ST_E_OK;
}
st_code st_sf_features(const st_sf_registry *r, const st_pr_row *row, st_sf_branch b, double *f, size_t *nf, st_error *err) {
  double t[16], w[16], m[8]; size_t nt, nw;
  st_code rc = st_pr_features(row->records, ST_PR_TEMPORAL, t, &nt, err);
  if (!rc) rc = st_pr_features(row->records, ST_PR_BRAID_WORD, w, &nw, err);
  st_sf_metadata md = {0};
  if (!rc) rc = st_sf_attached_metadata(r, row, &md, err);
  if (!rc) rc = st_sf_validate(r, &md, err);
  if (rc) return rc;
  for (int k = 0; k < 3; ++k) m[k] = !strcmp(md.source_id, SOURCE_ORDER[k]) ? 1. : 0.;
  m[3] = !strcmp(md.range_telemetry, "present"); m[4] = !strcmp(md.range_telemetry, "absent");
  m[5] = !strcmp(md.corroboration, "yes"); m[6] = !strcmp(md.corroboration, "no"); m[7] = md.sigma_m;
  size_t k = 0;
  switch (b) {
    case ST_SF_TEMPORAL: memcpy(f, t, nt * sizeof *t); k = nt; break;
    case ST_SF_TEMPORAL_SOURCE: memcpy(f, t, nt * sizeof *t); memcpy(f + nt, m, sizeof m); k = nt + 8; break;
    case ST_SF_TEMPORAL_BRAID: memcpy(f, t, nt * sizeof *t); memcpy(f + nt, w, nw * sizeof *w); k = nt + nw; break;
    case ST_SF_TEMPORAL_SOURCE_BRAID: memcpy(f, t, nt * sizeof *t); memcpy(f + nt, m, sizeof m); memcpy(f + nt + 8, w, nw * sizeof *w); k = nt + 8 + nw; break;
    case ST_SF_SOURCE_ONLY: memcpy(f, m, sizeof m); k = 8; break;
    default: return st_fail(err, ST_E_ARGUMENT, (long)b, "unknown branch");
  }
  *nf = k;
  return ST_E_OK;
}
typedef struct { const st_sf_registry *r; st_sf_branch b; } sf_ctx;
static st_code sf_extract(const void *ctx, const st_pr_row *row, double *f, size_t *nf, st_error *err) {
  const sf_ctx *c = ctx; return st_sf_features(c->r, row, c->b, f, nf, err);
}

st_code st_research_source_fusion(const char *root, st_json **out, st_error *err) {
  st_sf_registry reg; st_code rc = st_sf_load(root, &reg, err);
  if (rc) return rc;
  st_pr_data d; rc = st_pr_generate(&d, err);
  const char *names[5] = {"temporal", "temporal_source", "temporal_braid", "temporal_source_braid", "source_only"};
  st_json *o = st_json_object(), *branches = st_json_object();
  double mae[5] = {0};
  for (int k = 0; k < 5 && !rc; ++k) {
    sf_ctx c = {&reg, (st_sf_branch)k}; branch_eval e = {0};
    rc = evaluate_branch(&d, sf_extract, &c, &e, err);
    if (rc) break;
    double times[70], f[ST_PR_MAX_FEATURES]; size_t nf = 0; double pred[70], yt[70], cov[70];
    double total_ms = e.representation_s * 1e3 + e.tuning_s * 1e3, sum_ms = 0;
    for (size_t i = 0; i < d.ntest && !rc; ++i) {
      const st_pr_row *r = st_pr_split(&d, 2, i);
      double s = st_now_s();
      rc = st_sf_features(&reg, r, (st_sf_branch)k, f, &nf, err);
      pred[i] = st_ridge_predict1(&e.model, f);
      times[i] = (st_now_s() - s) * 1e3; sum_ms += times[i]; yt[i] = r->theta_rad_s;
    }
    if (rc) { free(e.pred); break; }
    for (size_t i = 0; i < d.ntest; ++i) cov[i] = fabs(pred[i] - yt[i]) <= e.q ? 1. : 0.;
    st_reg_metrics mm = st_reg_metrics_of(yt, pred, d.ntest); mae[k] = mm.mae;
    st_json *b = st_json_object();
    put_metrics(b, mm);
    st_json_set(b, "interval_90_coverage", st_json_double(st_np_mean(cov, d.ntest, 1)));
    st_json_set(b, "interval_90_width_rad_s", st_json_double(2 * e.q));
    st_json_set(b, "feature_count", st_json_int((int64_t)e.p));
    st_json_set(b, "alpha", st_json_double(e.model.alpha));
    st_json_set(b, "construction_ms_all_splits", st_json_double(e.representation_s * 1e3));
    st_json_set(b, "training_tuning_ms", st_json_double(e.tuning_s * 1e3));
    st_json_set(b, "p95_inference_ms", st_json_double(percentile_linear(times, d.ntest, 95)));
    st_json_set(b, "total_measured_ms", st_json_double(total_ms + sum_ms));
    st_json_set(branches, names[k], b);
    free(e.pred);
  }
  st_json *ex = st_json_object();
  if (!rc) {
    st_sf_metadata m = {0}; rc = st_sf_attached_metadata(&reg, st_pr_split(&d, 2, 0), &m, err);
    st_json_set(ex, "source_id", st_json_string(m.source_id));
    st_json_set(ex, "source_url", st_json_string(m.source_url));
    st_json_set(ex, "source_tier", st_json_string(m.source_tier));
    st_json_set(ex, "record_id", st_json_string(m.record_id));
    st_json_set(ex, "recording_origin_id", st_json_string(m.origin));
    st_json_set(ex, "source_link_status", st_json_string(m.link_status));
    st_json_set(ex, "range_telemetry", st_json_string(m.range_telemetry));
    st_json_set(ex, "corroboration", st_json_string(m.corroboration));
    st_json_set(ex, "resolution", st_json_string(m.resolution));
    st_json_set(ex, "sensor_modality", st_json_string("synthetic_position_meter_v1"));
    st_json_set(ex, "calibration_id", st_json_string(m.calibration_id));
    st_json_set(ex, "sigma_m", st_json_double(m.sigma_m));
  }
  st_json_set(o, "study", st_json_string("SKYTRACE 2.0 synthetic source-attribute fusion"));
  const st_json *ver = st_json_get(reg.registry, "registry_version");
  st_json_set(o, "source_registry_version", ver ? st_json_clone(ver) : st_json_null());
  st_json *ids = st_json_array(); for (int k = 0; k < 3; ++k) st_json_push(ids, st_json_string(SOURCE_ORDER[k]));
  st_json_set(o, "source_ids_used", ids);
  st_json_set(o, "source_association", st_json_string("synthetic, independent of omega"));
  st_json_set(o, "real_UAP_sensor_traces_loaded", st_json_int(0));
  st_json_set(o, "synthetic_trajectories", st_json_int(360));
  st_json *sp = st_json_object();
  st_json_set(sp, "train", st_json_int((int64_t)d.ntrain)); st_json_set(sp, "validation", st_json_int((int64_t)d.nval)); st_json_set(sp, "held_out", st_json_int((int64_t)d.ntest));
  st_json_set(o, "split", sp);
  st_json_set(o, "example_binding", ex);
  st_json_set(o, "branches", branches);
  st_json_set(o, "braid_incremental_mae_delta_rad_s", st_json_double(mae[2] - mae[0]));
  st_json_set(o, "source_incremental_mae_delta_rad_s", st_json_double(mae[1] - mae[0]));
  st_json_set(o, "validation_note", st_json_string("Archive metadata describe public source catalogs only. Associations with generated tracks are controlled simulations, not evidence of real videos."));
  st_pr_free(&d); st_sf_free(&reg);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}
