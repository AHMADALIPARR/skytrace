// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* skytrace research-export <dir>: interchange buffers for the BQN research cross-checks
 * (bqn/tests/research.bqn). Each buffer is an input the C port consumed plus what the C port
 * computed from it; BQN recomputes the latter independently. */
#include "cli.h"
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static st_code put(const char *dir, const char *name, const double *v, uint32_t rank, const uint64_t *shape, st_error *err) {
  st_ibuf b; st_code rc = st_ibuf_make(&b, ST_IBUF_F64, rank, shape, err);
  if (rc) return rc;
  memcpy(b.data, v, b.count * sizeof(double));
  char p[4400]; snprintf(p, sizeof p, "%s/%s.stbuf", dir, name);
  rc = st_ibuf_write(&b, p, err); st_ibuf_free(&b);
  return rc;
}
static st_code put1(const char *dir, const char *name, const double *v, size_t n, st_error *err) { uint64_t s[1] = {n}; return put(dir, name, v, 1, s, err); }
static st_code put2(const char *dir, const char *name, const double *v, size_t r, size_t c, st_error *err) { uint64_t s[2] = {r, c}; return put(dir, name, v, 2, s, err); }

static st_code split_matrix(const st_pr_data *d, int w, st_pr_branch b, double *X, double *y, size_t *p, st_error *err) {
  for (size_t i = 0; i < st_pr_split_n(d, w); ++i) {
    double f[ST_PR_MAX_FEATURES]; size_t nf = 0; const st_pr_row *r = st_pr_split(d, w, i);
    st_code rc = st_pr_features(r->records, b, f, &nf, err);
    if (rc) return rc;
    *p = nf; memcpy(X + i * nf, f, nf * sizeof *f); y[i] = r->theta_rad_s;
  }
  return ST_E_OK;
}

st_code cli_research_export(const char *dir, const char *root, st_error *err) {
  (void)root;
  st_code rc = st_mkdir_p(dir, err);
  if (rc) return rc;
  st_pr_data d; if ((rc = st_pr_generate(&d, err))) { st_pr_free(&d); return rc; }
  int nbuf = 0;
  /* one held-out track and its C feature vectors */
  const st_json *rec = st_pr_split(&d, 2, 0)->records->items[0];
  const st_json *ts = st_json_get(rec, "times_s"), *ps = st_json_get(rec, "positions_m");
  size_t n = ts->count; double *trk = malloc(n * 3 * sizeof *trk);
  for (size_t i = 0; i < n; ++i) { trk[3*i] = ts->items[i]->num; trk[3*i+1] = ps->items[i]->items[0]->num; trk[3*i+2] = ps->items[i]->items[1]->num; }
  rc = put2(dir, "pr_track", trk, n, 3, err); nbuf++; free(trk);
  static const char *FN[4] = {"pr_graph", "pr_temporal", "pr_braid_word", "pr_braid_timed"};
  for (int b = 0; b < 4 && !rc; ++b) {
    double f[ST_PR_MAX_FEATURES]; size_t nf = 0;
    rc = st_pr_features(st_pr_split(&d, 2, 0)->records, (st_pr_branch)b, f, &nf, err);
    if (!rc) { rc = put1(dir, FN[b], f, nf, err); nbuf++; }
  }
  /* ridge on the temporal branch: inputs, selected model, held-out predictions and metrics */
  double *X[3], *y[3]; size_t p = 0;
  for (int w = 0; w < 3; ++w) { X[w] = malloc(st_pr_split_n(&d, w) * 16 * sizeof(double)); y[w] = malloc(st_pr_split_n(&d, w) * sizeof(double)); }
  for (int w = 0; w < 3 && !rc; ++w) rc = split_matrix(&d, w, ST_PR_TEMPORAL, X[w], y[w], &p, err);
  static const char *XN[3] = {"pr_Xtr", "pr_Xv", "pr_Xt"}, *YN[3] = {"pr_ytr", "pr_yv", "pr_yt"};
  for (int w = 0; w < 3 && !rc; ++w) {
    rc = put2(dir, XN[w], X[w], st_pr_split_n(&d, w), p, err);
    if (!rc) rc = put1(dir, YN[w], y[w], st_pr_split_n(&d, w), err);
    nbuf += 2;
  }
  st_ridge m;
  if (!rc) rc = st_ridge_select(X[0], y[0], d.ntrain, X[1], y[1], d.nval, p, &m, err);
  if (!rc) {
    double model[1 + ST_RIDGE_MAX_P + 1], pred[70], ev[70], cov[70];
    model[0] = m.alpha; memcpy(model + 1, m.beta, (p + 1) * sizeof(double));
    for (size_t i = 0; i < d.ntest; ++i) pred[i] = st_ridge_predict1(&m, X[2] + i * p);
    for (size_t i = 0; i < d.nval; ++i) ev[i] = fabs(st_ridge_predict1(&m, X[1] + i * p) - y[1][i]);
    double q = st_np_quantile_higher(ev, d.nval, .9);
    for (size_t i = 0; i < d.ntest; ++i) cov[i] = fabs(pred[i] - y[2][i]) <= q;
    st_reg_metrics mm = st_reg_metrics_of(y[2], pred, d.ntest);
    double summary[5] = {mm.mae, mm.rmse, mm.bias, q, st_np_mean(cov, d.ntest, 1)};
    rc = put1(dir, "pr_model", model, p + 2, err);
    if (!rc) rc = put1(dir, "pr_pred", pred, d.ntest, err);
    if (!rc) rc = put1(dir, "pr_summary", summary, 5, err);
    nbuf += 3;
  }
  for (int w = 0; w < 3; ++w) { free(X[w]); free(y[w]); }
  st_pr_free(&d);
  /* joint fusion: one simulated scene, its fused triangulation and Fibonacci matrix features */
  if (!rc) {
    st_np_rng g; st_np_seed(&g, 42);
    st_jf_row row; rc = st_jf_simulate(2, &g, &row, err);
    double grid[ST_JF_GRID], xy[ST_JF_GRID][2]; int rank = 0;
    if (!rc) rc = st_jf_points(row.records, 1, 1, grid, xy, &rank, err);
    if (!rc) {
      /* per-sensor sample tables: (sensor, time, bearing, range or NaN) */
      size_t total = 0; for (size_t s = 0; s < row.records->count; ++s) total += st_json_get(row.records->items[s], "samples")->count;
      double *tab = malloc(total * 4 * sizeof *tab); size_t k = 0;
      for (size_t s = 0; s < row.records->count; ++s) {
        const st_json *sm = st_json_get(row.records->items[s], "samples");
        for (size_t i = 0; i < sm->count; ++i, ++k) {
          const st_json *r = st_json_get(sm->items[i], "range_m");
          tab[4*k] = (double)s; tab[4*k+1] = st_json_get(sm->items[i], "time_s")->num; tab[4*k+2] = st_json_get(sm->items[i], "bearing_rad")->num;
          tab[4*k+3] = r->type == ST_JNUM ? r->num : -1.0;   /* -1 = no range sample (ranges are > 0) */
        }
      }
      double pts[ST_JF_GRID * 3];
      for (size_t i = 0; i < ST_JF_GRID; ++i) { pts[3*i] = grid[i]; pts[3*i+1] = xy[i][0]; pts[3*i+2] = xy[i][1]; }
      double fib[8], sens[3 * 4];   /* per sensor: x, y, bearing sigma, range sigma */
      for (size_t s = 0; s < row.records->count && s < 3; ++s) {
        const st_json *r = row.records->items[s], *xy = st_json_get(r, "sensor_xy_m");
        sens[4*s] = xy->items[0]->num; sens[4*s+1] = xy->items[1]->num;
        sens[4*s+2] = st_json_get(r, "bearing_sigma_rad")->num; sens[4*s+3] = st_json_get(r, "range_sigma_m")->num;
      }
      rc = put2(dir, "jf_samples", tab, total, 4, err);
      if (!rc) rc = put2(dir, "jf_sensors", sens, row.records->count, 4, err);
      if (!rc) rc = put2(dir, "jf_points", pts, ST_JF_GRID, 3, err);
      if (!rc) rc = st_r22_braid_matrix_features(row.records, fib, err);
      if (!rc) rc = put1(dir, "r22_fib", fib, 8, err);
      nbuf += 4;
      free(tab);
    }
    st_jf_row_free(&row);
  }
  /* task information: one track's representation */
  if (!rc) {
    st_ti_row *rows = NULL; rc = st_ti_generate(200, 20261117u, &rows, err);
    if (!rc) {
      const st_json *r0 = rows[100].records->items[0], *a = st_json_get(r0, "angles");
      double ang[9], rep[4]; for (int i = 0; i < 9; ++i) ang[i] = a->items[i]->num;
      st_ti_repr r; rc = st_ti_representation(rows[100].records, &r, err);
      rep[0] = r.agnostic[0]; rep[1] = r.agnostic[1]; rep[2] = r.temporal; rep[3] = r.braid[0];
      if (!rc) rc = put1(dir, "ti_angles", ang, 9, err);
      if (!rc) rc = put1(dir, "ti_repr", rep, 4, err);
      nbuf += 2;
    }
    st_ti_free(rows, 200);
  }
  if (!rc) printf("exported %d research interchange buffers to %s\n", nbuf, dir);
  return rc;
}
