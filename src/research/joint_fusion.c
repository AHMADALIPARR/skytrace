// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* SKYTRACE 2.1 joint sensor/provenance calibration benchmark (port of research/joint_fusion/benchmark.py).
 * Three synthetic sensors (two bearing-only optical, one bearing+range radar) with unknown clock
 * offsets; per-time weighted triangulation on a common 12-point grid; ridge on the resulting
 * kinematic features. Synthetic only. */
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.141592653589793;
static const double SENSORS[3][2] = {{-22., -12.}, {24., -9.}, {-7., 27.}};

void st_jf_xy_path(double t, double x0, double y0, double v, double psi, double w, double *x, double *y) {
  if (fabs(w) < 1e-10) { *x = x0 + v * t * cos(psi); *y = y0 + v * t * sin(psi); return; }
  double s = st_np_sinc(w * t / (2 * PI));
  *x = x0 + v * t * s * cos(psi + w * t / 2);
  *y = y0 + v * t * s * sin(psi + w * t / 2);
}

st_code st_jf_simulate(int i, st_np_rng *g, st_jf_row *row, st_error *err) {
  const size_t n = 16; const double noise = 0.008, missing = 0.25, range_sigma = .22;
  double theta = st_np_uniform(g, -0.15, 0.15), v = st_np_uniform(g, 2.5, 5.0), psi = st_np_uniform(g, -PI, PI);
  double x0 = st_np_uniform(g, -3, 3), y0 = st_np_uniform(g, -3, 3), step = st_np_uniform(g, .38, .6);
  static const char *ORIG[3] = {"optical-A", "radar-B", "optical-C"};
  row->id = i; row->omega_rad_s = theta; row->records = st_json_array();
  if (!row->records) return st_fail(err, ST_E_NOMEM, -1, "simulate");
  for (int s = 0; s < 3; ++s) {
    double offset = st_np_uniform(g, -.12, .12), tr[16], bearing[16], ranges[16]; int valid[16];
    for (size_t k = 0; k < n; ++k) tr[k] = (double)k * step + offset;
    for (size_t k = 0; k < n; ++k) {
      double px, py; st_jf_xy_path(tr[k], x0, y0, v, psi, theta, &px, &py);
      double vx = px - SENSORS[s][0], vy = py - SENSORS[s][1];
      bearing[k] = atan2(vy, vx); ranges[k] = sqrt(vx * vx + vy * vy);
    }
    for (size_t k = 0; k < n; ++k) bearing[k] = bearing[k] + st_np_normal(g, 0, noise);
    for (size_t k = 0; k < n; ++k) ranges[k] = ranges[k] + st_np_normal(g, 0, range_sigma);
    for (size_t k = 0; k < n; ++k) valid[k] = s == 1 ? 1 : st_np_random(g) > missing;
    char id[48], cid[48], pid[48], cal[16];
    snprintf(id, sizeof id, "%d:%s", i, ORIG[s]); snprintf(cid, sizeof cid, "SYNTH-%d-%d", i, s);
    snprintf(pid, sizeof pid, "SYNTH-%d", i); snprintf(cal, sizeof cal, "CAL-%d", s);
    st_json *rec = st_json_object(), *prov = st_json_object(), *sxy = st_json_array(), *samples = st_json_array();
    st_json_set(prov, "archive", st_json_string("synthetic"));
    st_json_set(prov, "canonical_id", st_json_string(cid));
    st_json_set(prov, "parent_id", st_json_string(pid));
    st_json_set(prov, "calibration_id", st_json_string(cal));
    st_json_set(prov, "clock_offset_s", st_json_double(offset));
    st_json_push(sxy, st_json_double(SENSORS[s][0])); st_json_push(sxy, st_json_double(SENSORS[s][1]));
    for (size_t k = 0; k < n; ++k) {
      if (!valid[k]) continue;
      st_json *o = st_json_object();
      st_json_set(o, "frame", st_json_int((int64_t)k));
      st_json_set(o, "time_s", st_json_double(tr[k]));
      st_json_set(o, "bearing_rad", st_json_double(bearing[k]));
      st_json_set(o, "range_m", s == 1 ? st_json_double(ranges[k]) : st_json_null());
      st_json_push(samples, o);
    }
    st_json_set(rec, "origin_id", st_json_string(id));
    st_json_set(rec, "recording_origin", st_json_string(id));
    st_json_set(rec, "provenance", prov);
    st_json_set(rec, "sensor_xy_m", sxy);
    st_json_set(rec, "bearing_sigma_rad", st_json_double(noise));
    st_json_set(rec, "range_sigma_m", st_json_double(range_sigma));
    st_json_set(rec, "samples", samples);
    st_json_push(row->records, rec);
  }
  return ST_E_OK;
}
void st_jf_row_free(st_jf_row *r) { st_json_free(r->records); r->records = NULL; }

/* canon: exact duplicates (same canonical JSON digest) collapse; first-occurrence order kept. */
st_code st_jf_canon(const st_json *records, st_json **out, st_error *err) {
  if (!records || records->type != ST_JARR) return st_fail(err, ST_E_SCHEMA, -1, "records must be a list");
  char (*keys)[65] = calloc(records->count + 1, sizeof *keys); size_t nk = 0;
  const st_json **val = calloc(records->count + 1, sizeof *val);
  for (size_t i = 0; i < records->count; ++i) {
    char hex[65]; st_code rc = st_pr_record_digest(records->items[i], hex, err);
    if (rc) { free(keys); free(val); return rc; }
    size_t j = 0; while (j < nk && strcmp(keys[j], hex)) ++j;
    if (j == nk) { memcpy(keys[nk], hex, 65); nk++; }
    val[j] = records->items[i];
  }
  *out = st_json_array();
  for (size_t i = 0; i < nk; ++i) st_json_push(*out, st_json_clone(val[i]));
  free(keys); free(val);
  return ST_E_OK;
}

st_code st_jf_validate(const st_json *records, st_json **out, st_error *err) {
  st_json *rs = NULL; st_code rc = st_jf_canon(records, &rs, err);
  if (rc) return rc;
  for (size_t i = 0; i < rs->count; ++i)
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(st_json_get(rs->items[i], "origin_id")->str, st_json_get(rs->items[j], "origin_id")->str)) {
        st_json_free(rs); return st_fail(err, ST_E_CONFLICT, (long)i, "conflicting recording origin");
      }
  for (size_t i = 0; i < rs->count; ++i) {
    const st_json *r = rs->items[i], *s = st_json_get(r, "samples"), *xy = st_json_get(r, "sensor_xy_m");
    int ok = s && s->type == ST_JARR && s->count >= 5;
    for (size_t k = 0; ok && k + 1 < s->count; ++k) ok = st_json_get(s->items[k+1], "time_s")->num > st_json_get(s->items[k], "time_s")->num;
    if (!ok) { st_json_free(rs); return st_fail(err, ST_E_TIME, (long)i, "invalid time"); }
    if (!xy || xy->type != ST_JARR || xy->count != 2 || !isfinite(xy->items[0]->num) || !isfinite(xy->items[1]->num)) { st_json_free(rs); return st_fail(err, ST_E_FRAME, (long)i, "invalid geometry"); }
    const st_json *cid = st_json_get(st_json_get(r, "provenance"), "canonical_id");
    if (!cid || cid->type != ST_JSTR || cid->len == 0) { st_json_free(rs); return st_fail(err, ST_E_SCHEMA, (long)i, "missing provenance"); }
  }
  *out = rs;
  return ST_E_OK;
}

st_code st_jf_points(const st_json *records, int use_range, int use_multisensor, double grid[ST_JF_GRID],
                     double xy[ST_JF_GRID][2], int *rank, st_error *err) {
  st_json *rs = NULL; st_code rc = st_jf_validate(records, &rs, err);
  if (rc) return rc;
  size_t nr = use_multisensor ? rs->count : 1;
  double t0 = -INFINITY, t1 = INFINITY;
  for (size_t i = 0; i < nr; ++i) {
    const st_json *s = st_json_get(rs->items[i], "samples");
    t0 = fmax(t0, st_json_get(s->items[0], "time_s")->num);
    t1 = fmin(t1, st_json_get(s->items[s->count - 1], "time_s")->num);
  }
  st_np_linspace(t0, t1, ST_JF_GRID, grid);
  /* per-record arrays */
  size_t cap = 0; for (size_t i = 0; i < nr; ++i) { size_t c = st_json_get(rs->items[i], "samples")->count; if (c > cap) cap = c; }
  double *times = malloc(nr * cap * sizeof *times), *bear = malloc(nr * cap * sizeof *bear), *raw = malloc(cap * sizeof *raw);
  double *tr = malloc(nr * cap * sizeof *tr), *rv = malloc(nr * cap * sizeof *rv);
  size_t *ns = calloc(nr, sizeof *ns), *nrg = calloc(nr, sizeof *nrg);
  double *A = malloc(2 * nr * 2 * sizeof *A), *b = malloc(2 * nr * sizeof *b), *w = malloc(2 * nr * sizeof *w);
  for (size_t i = 0; i < nr; ++i) {
    const st_json *s = st_json_get(rs->items[i], "samples");
    ns[i] = s->count;
    for (size_t k = 0; k < s->count; ++k) {
      times[i*cap+k] = st_json_get(s->items[k], "time_s")->num; raw[k] = st_json_get(s->items[k], "bearing_rad")->num;
      const st_json *r = st_json_get(s->items[k], "range_m");
      if (r && r->type == ST_JNUM) { tr[i*cap+nrg[i]] = times[i*cap+k]; rv[i*cap+nrg[i]] = r->num; nrg[i]++; }
    }
    st_np_unwrap(raw, s->count, bear + i * cap);
  }
  int minrank = 2;
  for (size_t g = 0; g < ST_JF_GRID && !rc; ++g) {
    size_t m = 0;
    for (size_t i = 0; i < nr; ++i) {
      const st_json *r = rs->items[i], *sxy = st_json_get(r, "sensor_xy_m");
      double sx = sxy->items[0]->num, sy = sxy->items[1]->num;
      double angle = st_np_interp(grid[g], times + i * cap, bear + i * cap, ns[i]);
      double nx = -sin(angle), ny = cos(angle);
      double bs = st_json_get(r, "bearing_sigma_rad")->num, rsg = st_json_get(r, "range_sigma_m")->num;
      A[2*m] = nx; A[2*m+1] = ny; b[m] = nx * sx + ny * sy; w[m] = 1 / pow(fmax(bs, 1e-6), 2.0); ++m;
      if (use_range && nrg[i] > 0) {
        double rr = st_np_interp(grid[g], tr + i * cap, rv + i * cap, nrg[i]), ux = cos(angle), uy = sin(angle);
        A[2*m] = ux; A[2*m+1] = uy; b[m] = ux * sx + uy * sy + rr; w[m] = 1 / pow(fmax(rsg, .01), 2.0); ++m;
      }
    }
    double Aw[64], bw[32];
    for (size_t k = 0; k < m; ++k) { double s = sqrt(w[k]); Aw[2*k] = A[2*k] * s; Aw[2*k+1] = A[2*k+1] * s; bw[k] = b[k] * s; }
    int rk = st_matrix_rank2(A, m);
    if (rk < minrank) minrank = rk;
    rc = st_lstsq2(Aw, bw, m, xy[g], err);
  }
  *rank = minrank;
  free(times); free(bear); free(raw); free(tr); free(rv); free(ns); free(nrg); free(A); free(b); free(w);
  st_json_free(rs);
  return rc;
}

st_code st_jf_features(const st_json *records, st_jf_branch br, double *f, size_t *nf, st_error *err) {
  if (br == ST_JF_BEARING_ONLY) {
    st_json *rs = NULL; st_code rc = st_jf_validate(records, &rs, err);
    if (rc) return rc;
    const st_json *s = st_json_get(rs->items[0], "samples");
    size_t n = s->count;
    double *t = malloc(n * sizeof *t), *raw = malloc(n * sizeof *raw), *a = malloc(n * sizeof *a), *d = malloc(n * sizeof *d);
    for (size_t k = 0; k < n; ++k) { t[k] = st_json_get(s->items[k], "time_s")->num; raw[k] = st_json_get(s->items[k], "bearing_rad")->num; }
    st_np_unwrap(raw, n, a);
    for (size_t k = 0; k + 1 < n; ++k) d[k] = (a[k+1] - a[k]) / (t[k+1] - t[k]);
    f[0] = st_polyfit_slope(t, a, n); f[1] = st_np_mean(d, n - 1, 1); f[2] = st_np_std(d, n - 1, 1);
    f[3] = st_np_median(d, n - 1); f[4] = t[n-1] - t[0]; f[5] = (double)n;
    *nf = 6;
    free(t); free(raw); free(a); free(d); st_json_free(rs);
    return ST_E_OK;
  }
  double grid[ST_JF_GRID], xy[ST_JF_GRID][2]; int rank = 0;
  st_code rc = st_jf_points(records, br != ST_JF_MULTIBEARING, 1, grid, xy, &rank, err);
  if (rc) return rc;
  const size_t G = ST_JF_GRID;
  double raw[ST_JF_GRID], h[ST_JF_GRID], tm[ST_JF_GRID], rates[ST_JF_GRID], sp[ST_JF_GRID];
  for (size_t k = 0; k + 1 < G; ++k) {
    double dx = xy[k+1][0] - xy[k][0], dy = xy[k+1][1] - xy[k][1];
    raw[k] = atan2(dy, dx); tm[k] = (grid[k+1] + grid[k]) / 2;
    sp[k] = sqrt(dx * dx + dy * dy) / (grid[k+1] - grid[k]);
  }
  st_np_unwrap(raw, G - 1, h);
  for (size_t k = 0; k + 2 < G; ++k) rates[k] = (h[k+1] - h[k]) / (tm[k+1] - tm[k]);
  size_t nrt = G - 2;
  f[0] = st_polyfit_slope(tm, h, G - 1); f[1] = st_np_mean(rates, nrt, 1); f[2] = st_np_median(rates, nrt);
  f[3] = st_np_std(rates, nrt, 1); f[4] = st_np_mean(sp, G - 1, 1); f[5] = grid[G-1] - grid[0]; f[6] = rank;
  *nf = 7;
  if (br != ST_JF_FUSED_BRAID) return ST_E_OK;
  double s[ST_JF_GRID], w[ST_JF_GRID], aw[ST_JF_GRID], ch[ST_JF_GRID], pr[ST_JF_GRID];
  for (size_t k = 0; k < nrt; ++k) { s[k] = st_np_sign(rates[k]); w[k] = s[k] * (k % 2 == 0 ? 1. : 2.); aw[k] = fabs(w[k]); }
  for (size_t k = 0; k + 1 < nrt; ++k) { ch[k] = (s[k+1] - s[k]) != 0 ? 1. : 0.; pr[k] = s[k] * s[k+1]; }
  f[7] = st_np_mean(s, nrt, 1); f[8] = st_np_mean(ch, nrt - 1, 1); f[9] = st_np_sum(w, nrt, 1); f[10] = st_np_sum(aw, nrt, 1);
  f[11] = st_np_mean(pr, nrt - 1, 1);
  *nf = 12;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- evaluation */
typedef struct { st_jf_row *rows; size_t n; size_t ids[180]; } jf_data;
static st_code jf_generate(uint64_t seed, size_t n, jf_data *d, st_error *err) {
  st_np_rng g; st_np_seed(&g, seed);
  d->rows = calloc(n, sizeof *d->rows); d->n = n;
  for (size_t i = 0; i < n; ++i) { st_code rc = st_jf_simulate((int)i, &g, &d->rows[i], err); if (rc) return rc; }
  st_np_permutation(&g, n, d->ids);
  return ST_E_OK;
}
static void jf_free(jf_data *d) { for (size_t i = 0; d->rows && i < d->n; ++i) st_jf_row_free(&d->rows[i]); free(d->rows); d->rows = NULL; }

typedef st_code (*jf_extract)(const st_json *records, int branch, double *f, size_t *nf, st_error *err);
static st_code jf_plain(const st_json *r, int b, double *f, size_t *nf, st_error *err) { return st_jf_features(r, (st_jf_branch)b, f, nf, err); }

typedef struct { st_reg_metrics m; double alpha, coverage, width, construction_s, training_s; size_t p; } jf_eval;
static st_code fit_branch(const jf_data *d, const size_t *tr, size_t ntr, const size_t *va, size_t nva, const size_t *te, size_t nte,
                          jf_extract fx, int branch, jf_eval *o, st_error *err) {
  const size_t *sets[3] = {tr, va, te}; size_t ns[3] = {ntr, nva, nte};
  double *X[3], *y[3]; size_t p = 0; st_code rc = ST_E_OK;
  double tic = st_now_s();
  for (int s = 0; s < 3; ++s) {
    X[s] = malloc(ns[s] * 64 * sizeof(double)); y[s] = malloc(ns[s] * sizeof(double));
    for (size_t i = 0; i < ns[s] && !rc; ++i) {
      double f[64]; size_t nf = 0; const st_jf_row *r = &d->rows[sets[s][i]];
      rc = fx(r->records, branch, f, &nf, err);
      if (rc) break;
      p = nf; memcpy(X[s] + i * nf, f, nf * sizeof *f); y[s][i] = r->omega_rad_s;
    }
  }
  o->construction_s = st_now_s() - tic;
  st_ridge best;
  if (!rc) { tic = st_now_s(); rc = st_ridge_select(X[0], y[0], ntr, X[1], y[1], nva, p, &best, err); o->training_s = st_now_s() - tic; }
  if (!rc) {
    double *pred = malloc(nte * sizeof *pred), *ev = malloc(nva * sizeof *ev), *cov = malloc(nte * sizeof *cov);
    for (size_t i = 0; i < nte; ++i) pred[i] = st_ridge_predict1(&best, X[2] + i * p);
    for (size_t i = 0; i < nva; ++i) ev[i] = fabs(st_ridge_predict1(&best, X[1] + i * p) - y[1][i]);
    double q = st_np_quantile_higher(ev, nva, .9);
    for (size_t i = 0; i < nte; ++i) cov[i] = fabs(pred[i] - y[2][i]) <= q ? 1. : 0.;
    o->m = st_reg_metrics_of(y[2], pred, nte); o->alpha = best.alpha; o->coverage = st_np_mean(cov, nte, 1); o->width = 2 * q; o->p = p;
    free(pred); free(ev); free(cov);
  }
  for (int s = 0; s < 3; ++s) { free(X[s]); free(y[s]); }
  return rc;
}

static const char *JF_NAMES[4] = {"bearing_only", "multibearing", "fused", "fused_braid"};

st_code st_research_joint_fusion(uint64_t seed, st_json **out, st_error *err) {
  jf_data d = {0}; st_code rc = jf_generate(seed, 180, &d, err);
  st_json *o = st_json_object(), *branches = st_json_object();
  st_json_set(o, "experiment", st_json_string("SKYTRACE 2.1 joint metadata/sensor fusion"));
  st_json_set(o, "seed", st_json_int((int64_t)seed));
  st_json_set(o, "synthetic", st_json_bool(1));
  st_json *sp = st_json_object();
  st_json_set(sp, "train", st_json_int(100)); st_json_set(sp, "validation", st_json_int(40)); st_json_set(sp, "test", st_json_int(40));
  st_json_set(o, "split", sp);
  double tic = st_now_s();
  for (int b = 0; b < 4 && !rc; ++b) {
    jf_eval e = {0}; rc = fit_branch(&d, d.ids, 100, d.ids + 100, 40, d.ids + 140, 40, jf_plain, b, &e, err);
    if (rc) break;
    st_json *x = st_json_object();
    st_json_set(x, "mae_rad_s", st_json_double(e.m.mae)); st_json_set(x, "rmse_rad_s", st_json_double(e.m.rmse)); st_json_set(x, "bias_rad_s", st_json_double(e.m.bias));
    st_json_set(x, "alpha", st_json_double(e.alpha)); st_json_set(x, "coverage90", st_json_double(e.coverage));
    st_json_set(x, "interval90_width", st_json_double(e.width)); st_json_set(x, "feature_count", st_json_int((int64_t)e.p));
    st_json_set(x, "construction_all_s", st_json_double(e.construction_s)); st_json_set(x, "training_s", st_json_double(e.training_s));
    st_json_set(x, "total_s", st_json_double(e.construction_s + e.training_s));
    st_json_set(branches, JF_NAMES[b], x);
  }
  st_json_set(o, "branches", branches);
  st_json_set(o, "wall_s", st_json_double(st_now_s() - tic));
  if (!rc) {
    const st_json *rec = d.rows[d.ids[140]].records;
    st_json *dupe = st_json_clone(rec); st_json_push(dupe, st_json_clone(rec->items[0]));
    double f1[16], f2[16]; size_t n1, n2; st_json *val = NULL; double grid[ST_JF_GRID], xy[ST_JF_GRID][2]; int rank = 0;
    rc = st_jf_features(rec, ST_JF_FUSED_BRAID, f1, &n1, err);
    if (!rc) rc = st_jf_features(dupe, ST_JF_FUSED_BRAID, f2, &n2, err);
    if (!rc) rc = st_jf_validate(rec, &val, err);
    if (!rc) rc = st_jf_points(rec, 1, 1, grid, xy, &rank, err);
    if (!rc) {
      int close = n1 == n2; for (size_t i = 0; close && i < n1; ++i) close = fabs(f1[i] - f2[i]) <= 1e-8 + 1e-5 * fabs(f2[i]);
      size_t distinct = 0;
      for (size_t i = 0; i < val->count; ++i) { int seen = 0; for (size_t j = 0; j < i; ++j) seen |= !strcmp(st_json_get(val->items[i], "origin_id")->str, st_json_get(val->items[j], "origin_id")->str); distinct += !seen; }
      double nrm = 0; for (size_t i = 7; i < n1; ++i) nrm += f1[i] * f1[i];
      st_json *c = st_json_object();
      st_json_set(c, "duplicate_invariant", st_json_bool(close));
      st_json_set(c, "distinct_origins", st_json_int((int64_t)distinct));
      st_json_set(c, "calibrated_multisensor_rank", st_json_int(rank));
      st_json_set(c, "braid_delta_features_norm", st_json_double(sqrt(nrm)));
      st_json_set(c, "source_id_sample", st_json_clone(st_json_get(st_json_get(rec->items[0], "provenance"), "canonical_id")));
      st_json_set(o, "controls", c);
    }
    st_json_free(dupe); st_json_free(val);
  }
  jf_free(&d);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- release 2.2 */
st_code st_r22_braid_matrix_features(const st_json *records, double f[8], st_error *err) {
  double grid[ST_JF_GRID], xy[ST_JF_GRID][2]; int rank = 0;
  st_code rc = st_jf_points(records, 1, 1, grid, xy, &rank, err);
  if (rc) return rc;
  const size_t G = ST_JF_GRID; double raw[ST_JF_GRID], h[ST_JF_GRID], tm[ST_JF_GRID];
  for (size_t k = 0; k + 1 < G; ++k) { raw[k] = atan2(xy[k+1][1] - xy[k][1], xy[k+1][0] - xy[k][0]); tm[k] = (grid[k+1] + grid[k]) / 2; }
  st_np_unwrap(raw, G - 1, h);
  int word[ST_JF_GRID];
  for (size_t k = 0; k + 2 < G; ++k) { double r = (h[k+1] - h[k]) / (tm[k+1] - tm[k]); word[k] = (k % 2 == 0 ? 1 : 2) * (r >= 0 ? 1 : -1); }
  st_mat2 U; if ((rc = st_braid_word_matrix(word, G - 2, &U, err))) return rc;
  if (st_mat2_unitarity_error(U) > 1e-10) return st_fail(err, ST_E_TOLERANCE, -1, "invalid braid unitary");
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) { f[i*2+j] = creal(U.m[i][j]); f[4+i*2+j] = cimag(U.m[i][j]); }
  return ST_E_OK;
}

void st_r22_physical_fixture(uint64_t seed, st_r22_fixture *f) {
  st_np_rng g; st_np_seed(&g, seed);
  const double noise = .003; const size_t n = 20;
  f->truth[0] = st_np_uniform(&g, -2, 2); f->truth[1] = st_np_uniform(&g, -2, 2); f->truth[2] = st_np_uniform(&g, 3, 4.5);
  f->truth[3] = st_np_uniform(&g, -1.2, 1.2); f->truth[4] = st_np_uniform(&g, -.11, .11);
  f->offsets[0] = 0.; f->offsets[1] = st_np_uniform(&g, -.18, .18); f->offsets[2] = st_np_uniform(&g, -.18, .18);
  double t[20]; st_np_linspace(0, 6, n, t);
  size_t k = 0;
  for (int i = 0; i < 3; ++i) {
    double b[20], r[20];
    for (size_t j = 0; j < n; ++j) {
      double x, y; st_jf_xy_path(t[j], f->truth[0], f->truth[1], f->truth[2], f->truth[3], f->truth[4], &x, &y);
      double dx = x - SENSORS[i][0], dy = y - SENSORS[i][1];
      b[j] = atan2(dy, dx); r[j] = sqrt(dx * dx + dy * dy);
    }
    for (size_t j = 0; j < n; ++j) b[j] = b[j] + st_np_normal(&g, 0, noise);
    for (size_t j = 0; j < n; ++j) r[j] = r[j] + st_np_normal(&g, 0, .10);
    for (size_t j = 0; j < n; ++j) { f->rows[k][0] = i; f->rows[k][1] = t[j] + f->offsets[i]; f->rows[k][2] = b[j]; f->rows[k][3] = r[j]; ++k; }
  }
}

/* braid_control: 'temporal' = fused features; 'fibonacci_matrix' = fused + Fibonacci matrix features */
static st_code r22_extract(const st_json *records, int branch, double *f, size_t *nf, st_error *err) {
  st_code rc = st_jf_features(records, ST_JF_FUSED, f, nf, err);
  if (rc || branch == 0) return rc;
  rc = st_r22_braid_matrix_features(records, f + *nf, err);
  *nf += 8;
  return rc;
}
static st_code braid_control(st_json **out, st_error *err) {
  jf_data d = {0}; st_code rc = jf_generate(20261117u, 90, &d, err);
  st_json *o = st_json_object();
  const char *names[2] = {"temporal", "fibonacci_matrix"};
  for (int b = 0; b < 2 && !rc; ++b) {
    jf_eval e = {0}; double tic = st_now_s();
    rc = fit_branch(&d, d.ids, 50, d.ids + 50, 20, d.ids + 70, 20, r22_extract, b, &e, err);
    if (rc) break;
    st_json *x = st_json_object();
    st_json_set(x, "mae_rad_s", st_json_double(e.m.mae)); st_json_set(x, "alpha", st_json_double(e.alpha));
    st_json_set(x, "construction_s", st_json_double(e.construction_s)); st_json_set(x, "training_s", st_json_double(e.training_s));
    st_json_set(x, "prediction_s", st_json_double(st_now_s() - tic - e.construction_s - e.training_s));
    st_json_set(x, "features", st_json_int((int64_t)e.p));
    st_json_set(o, names[b], x);
  }
  jf_free(&d);
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}

static double traj_rmse(const double *est, const double *truth) {
  double t[20], s[20]; st_np_linspace(0, 6, 20, t);
  for (int k = 0; k < 20; ++k) {
    double xe, ye, xt, yt;
    st_jf_xy_path(t[k], est[0], est[1], est[2], est[3], est[4], &xe, &ye);
    st_jf_xy_path(t[k], truth[0], truth[1], truth[2], truth[3], truth[4], &xt, &yt);
    s[k] = (xe - xt) * (xe - xt) + (ye - yt) * (ye - yt);
  }
  return sqrt(st_np_mean(s, 20, 1));
}

#include "skytrace/fusion.h"
st_code st_r22_fit_physics(const st_r22_fixture *fx, int known, st_r22_fit *o, st_error *err) {
  st_ct_row rows[60];
  for (int i = 0; i < 60; ++i) {
    int s = (int)fx->rows[i][0];
    double t = fx->rows[i][1] - (known ? fx->offsets[s] : 0);
    rows[i] = (st_ct_row){ s, t, SENSORS[s][0], SENSORS[s][1], fx->rows[i][2], fx->rows[i][3] };
  }
  st_ct_problem pb = { rows, 60, 3, 0.003, 0.10, !known, 1 };
  double init[ST_CT_MAX_PARAMS]; st_ct_options op; st_ct_default_options(&op); st_ct_result res;
  st_code rc = st_ct_initial_guess(&pb, init, err);
  if (!rc) rc = st_ct_fit(&pb, init, &op, &res, err);
  if (rc) return rc;
  double est[3] = {0, known ? fx->offsets[1] : res.x[5], known ? fx->offsets[2] : res.x[6]};
  o->omega_error = fabs(res.x[4] - fx->truth[4]);
  o->clock_mae_s = (fabs(est[1] - fx->offsets[1]) + fabs(est[2] - fx->offsets[2])) / 2;
  o->trajectory_rmse_m = traj_rmse(res.x, fx->truth);
  o->converged = res.converged && res.residual_norm < 250;
  o->nfev = res.nfev;
  return ST_E_OK;
}
static st_code fit_physics(const st_r22_fixture *fx, int known, st_json **out, double agg[4], int *failed, st_error *err) {
  st_r22_fit f; st_code rc = st_r22_fit_physics(fx, known, &f, err);
  if (rc) return rc;
  st_json *m = st_json_object();
  st_json_set(m, "omega_error", st_json_double(f.omega_error)); st_json_set(m, "clock_mae_s", st_json_double(f.clock_mae_s));
  st_json_set(m, "trajectory_rmse_m", st_json_double(f.trajectory_rmse_m)); st_json_set(m, "converged", st_json_bool(f.converged));
  st_json_set(m, "nfev", st_json_int(f.nfev));
  agg[0] += f.omega_error; agg[1] += f.clock_mae_s; agg[2] += f.trajectory_rmse_m; agg[3] += f.nfev; *failed += !f.converged;
  *out = m;
  return ST_E_OK;
}

st_code st_research_release22(const char *root, st_json **out, st_error *err) {
  double begun = st_now_s();
  char path[4096]; snprintf(path, sizeof path, "%s/research/joint_fusion/results.synthetic.json", root);
  st_json *orig = st_json_parse_file(path, err);
  if (!orig) return err ? err->code : ST_E_IO;
  st_json *frozen = NULL; st_code rc = st_research_joint_fusion(20261117u, &frozen, err);
  st_json *o = st_json_object(), *diff = st_json_object();
  for (int b = 0; b < 4 && !rc; ++b) {
    double a = st_json_get(st_json_get(st_json_get(frozen, "branches"), JF_NAMES[b]), "mae_rad_s")->num;
    double c = st_json_get(st_json_get(st_json_get(orig, "branches"), JF_NAMES[b]), "mae_rad_s")->num;
    st_json_set(diff, JF_NAMES[b], st_json_double(a - c));
  }
  st_json_free(frozen); st_json_free(orig);
  st_json *seeds = st_json_array();
  double mae[4][5], cov[4][5], wid[4][5];
  for (int s = 0; s < 5 && !rc; ++s) {
    st_json *r = NULL; rc = st_research_joint_fusion(20261117u + (uint64_t)s, &r, err);
    if (rc) break;
    st_json *ps = st_json_object(), *methods = st_json_object();
    st_json_set(ps, "seed", st_json_int(20261117 + s));
    for (int b = 0; b < 4; ++b) {
      const st_json *x = st_json_get(st_json_get(r, "branches"), JF_NAMES[b]);
      mae[b][s] = st_json_get(x, "mae_rad_s")->num; cov[b][s] = st_json_get(x, "coverage90")->num; wid[b][s] = st_json_get(x, "interval90_width")->num;
      st_json *m = st_json_object();
      st_json_set(m, "mae_rad_s", st_json_double(mae[b][s])); st_json_set(m, "coverage90", st_json_double(cov[b][s]));
      st_json_set(m, "interval90_width_rad_s", st_json_double(wid[b][s]));
      st_json_set(methods, JF_NAMES[b], m);
    }
    st_json_set(ps, "methods", methods); st_json_push(seeds, ps); st_json_free(r);
  }
  st_json *agg = st_json_object();
  for (int b = 0; b < 4 && !rc; ++b) {
    double mean = st_np_mean(mae[b], 5, 1), ss = 0;
    for (int s = 0; s < 5; ++s) ss += (mae[b][s] - mean) * (mae[b][s] - mean);
    double sd = sqrt(ss / 4.0), half = 2.776 * sd / sqrt(5.0);
    st_json *a = st_json_object(), *ci = st_json_array();
    st_json_set(a, "mean_seed_mae_rad_s", st_json_double(mean)); st_json_set(a, "sd_seed_mae_rad_s", st_json_double(sd));
    st_json_push(ci, st_json_double(mean - half)); st_json_push(ci, st_json_double(mean + half));
    st_json_set(a, "approx_95ci_mean_mae_rad_s", ci);
    st_json_set(a, "mean_coverage90", st_json_double(st_np_mean(cov[b], 5, 1)));
    st_json_set(a, "mean_interval90_width_rad_s", st_json_double(st_np_mean(wid[b], 5, 1)));
    st_json_set(agg, JF_NAMES[b], a);
  }
  st_json *phys = st_json_array(); double pa[2][4] = {{0}}; int failed[2] = {0, 0};
  for (int s = 0; s < 12 && !rc; ++s) {
    st_r22_fixture fx; st_r22_physical_fixture((uint64_t)s, &fx);
    st_json *row = st_json_object(), *k = NULL, *u = NULL;
    st_json_set(row, "seed", st_json_int(s));
    rc = fit_physics(&fx, 1, &k, pa[0], &failed[0], err);
    if (!rc) rc = fit_physics(&fx, 0, &u, pa[1], &failed[1], err);
    st_json_set(row, "known", k ? k : st_json_null()); st_json_set(row, "unknown", u ? u : st_json_null());
    st_json_push(phys, row);
  }
  st_json *pag = st_json_object();
  static const char *PK[4] = {"omega_error", "clock_mae_s", "trajectory_rmse_m", "nfev"};
  for (int m = 0; m < 2; ++m) {
    st_json *a = st_json_object();
    for (int k = 0; k < 4; ++k) st_json_set(a, PK[k], st_json_double(pa[m][k] / 12.0));
    st_json_set(a, "failed", st_json_int(failed[m]));
    st_json_set(pag, m ? "unknown" : "known", a);
  }
  st_json *braid = NULL; if (!rc) rc = braid_control(&braid, err);
  st_json_set(o, "version", st_json_string("2.2"));
  st_json_set(o, "synthetic", st_json_bool(1));
  st_json_set(o, "frozen_21_mae_differences", diff);
  st_json_set(o, "frozen_tolerance", st_json_double(1e-10));
  st_json_set(o, "seeds", seeds);
  st_json_set(o, "aggregate", agg);
  st_json_set(o, "physical_fit", phys);
  st_json_set(o, "physical_aggregate", pag);
  st_json_set(o, "braid_ablation", braid ? braid : st_json_null());
  st_json *env = st_json_object();
  st_json_set(env, "implementation", st_json_string("C11 (skytrace research release22); physical fits use the Levenberg-Marquardt solver of src/fusion"));
  st_json_set(o, "environment", env);
  st_json_set(o, "wall_seconds", st_json_double(st_now_s() - begun));
  if (rc) { st_json_free(o); return rc; }
  *out = o;
  return ST_E_OK;
}
