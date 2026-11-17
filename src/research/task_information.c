// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Pre-registered synthetic task-information benchmark (port of research/task_information/benchmark.py).
 * Angular tracks come from CPython's random.Random(20261117) stream (MT19937, reproduced bit for bit);
 * fixed-rule classifiers, no fitted thresholds. Python's built-in sum() over floats is compensated
 * (Neumaier, CPython >= 3.12) and is reproduced as such. No real UAP claims. */
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double py_sum(const double *x, size_t n) {
  double r = 0.0, c = 0.0;
  for (size_t i = 0; i < n; ++i) {
    double t = r + x[i];
    if (fabs(r) >= fabs(x[i])) c += (r - t) + x[i]; else c += (x[i] - t) + r;
    r = t;
  }
  if (c != 0 && isfinite(c)) r += c;
  return r;
}

static st_code digest(const st_json *v, char hex[65], st_error *err) { return st_pr_record_digest(v, hex, err); }

st_code st_ti_generate(size_t n, uint64_t seed, st_ti_row **rows, st_error *err) {
  st_py_rng r; st_py_seed(&r, seed);
  st_ti_row *out = calloc(n ? n : 1, sizeof *out);
  if (!out) return st_fail(err, ST_E_NOMEM, -1, "generate");
  for (size_t i = 0; i < n; ++i) {
    int label = (int)(i % 2); double sign = label ? 1 : -1;
    double velocity = sign * st_py_uniform(&r, 0.012, 0.035);
    double intercept = st_py_uniform(&r, -.5, .5), noise[9];
    for (int k = 0; k < 9; ++k) noise[k] = st_py_gauss(&r, 0, .002);
    char origin[32]; snprintf(origin, sizeof origin, "origin-%zu", i);
    st_json *rec = st_json_object(), *times = st_json_array(), *angles = st_json_array();
    for (int t = 0; t < 9; ++t) { st_json_push(times, st_json_int(t)); st_json_push(angles, st_json_double(intercept + velocity * (double)t + noise[t])); }
    st_json_set(rec, "origin", st_json_string(origin));
    st_json_set(rec, "calibration", st_json_string("calib-v1"));
    st_json_set(rec, "uncertainty", st_json_double(.002));
    st_json_set(rec, "times", times);
    st_json_set(rec, "angles", angles);
    st_json_set(rec, "range_m", st_json_null());
    out[i].label = label; out[i].records = st_json_array(); st_json_push(out[i].records, rec);
  }
  st_py_shuffle(&r, out, n, sizeof *out);
  *rows = out;
  return ST_E_OK;
}
void st_ti_free(st_ti_row *rows, size_t n) { for (size_t i = 0; rows && i < n; ++i) st_json_free(rows[i].records); free(rows); }

typedef struct { char hex[65]; st_json *val; } keyed;
static int cmpk(const void *a, const void *b) { return strcmp(((const keyed *)a)->hex, ((const keyed *)b)->hex); }
st_code st_ti_canonical_origins(const st_json *records, st_json **out, st_error *err) {
  static const char *KEYS[6] = {"origin", "calibration", "uncertainty", "times", "angles", "range_m"};
  keyed *k = calloc(records->count + 1, sizeof *k); size_t nk = 0;
  for (size_t i = 0; i < records->count; ++i) {
    st_json *v = st_json_object();
    for (int j = 0; j < 6; ++j) {
      const st_json *x = st_json_get(records->items[i], KEYS[j]);
      if (!x) { st_json_free(v); for (size_t q = 0; q < nk; ++q) st_json_free(k[q].val); free(k); return st_fail(err, ST_E_SCHEMA, (long)i, "record missing %s", KEYS[j]); }
      st_json_set(v, KEYS[j], st_json_clone(x));
    }
    char hex[65]; st_code rc = digest(v, hex, err);
    if (rc) { st_json_free(v); for (size_t q = 0; q < nk; ++q) st_json_free(k[q].val); free(k); return rc; }
    size_t j = 0; while (j < nk && strcmp(k[j].hex, hex)) ++j;
    if (j == nk) { memcpy(k[nk].hex, hex, 65); k[nk].val = NULL; nk++; }
    st_json_free(k[j].val); k[j].val = v;
  }
  qsort(k, nk, sizeof *k, cmpk);
  *out = st_json_array();
  for (size_t i = 0; i < nk; ++i) st_json_push(*out, k[i].val);
  free(k);
  return ST_E_OK;
}

st_code st_ti_representation(const st_json *records, st_ti_repr *rp, st_error *err) {
  memset(rp, 0, sizeof *rp);
  st_json *rs = NULL; st_code rc = st_ti_canonical_origins(records, &rs, err);
  if (rc) return rc;
  double inc[64]; size_t ni = 0;
  for (size_t i = 0; i < rs->count; ++i) {
    const st_json *t = st_json_get(rs->items[i], "times"), *a = st_json_get(rs->items[i], "angles");
    int ok = t && a && t->type == ST_JARR && a->type == ST_JARR && t->count == a->count && t->count >= 2;
    for (size_t k = 0; ok && k + 1 < t->count; ++k) ok = t->items[k+1]->num > t->items[k]->num;
    if (!ok) { st_json_free(rs); return st_fail(err, ST_E_TIME, (long)i, "invalid track"); }
    for (size_t k = 0; k + 1 < t->count; ++k) {
      if (ni >= 64) { st_json_free(rs); return st_fail(err, ST_E_BOUND, -1, "too many samples"); }
      inc[ni++] = (a->items[k+1]->num - a->items[k]->num) / (t->items[k+1]->num - t->items[k]->num);
    }
  }
  if (!ni) { st_json_free(rs); return st_fail(err, ST_E_SCHEMA, -1, "no samples"); }
  double ab[64], sq[64];
  for (size_t i = 0; i < ni; ++i) { ab[i] = fabs(inc[i]); sq[i] = inc[i] * inc[i]; }
  rp->agnostic[0] = py_sum(ab, ni) / (double)ni; rp->agnostic[1] = py_sum(sq, ni) / (double)ni;
  rp->temporal = py_sum(inc, ni) / (double)ni;
  long sgn = 0;
  for (size_t i = 0; i < ni; ++i) { rp->word[i] = (inc[i] >= 0 ? 1 : -1) * (i % 2 == 0 ? 1 : 2); sgn += rp->word[i] > 0 ? 1 : -1; }
  rp->nword = ni;
  st_mat2 U; if ((rc = st_braid_word_matrix(rp->word, ni, &U, err))) { st_json_free(rs); return rc; }
  double complex tr = U.m[0][0] + U.m[1][1];
  rp->braid[0] = (double)sgn / (double)ni; rp->braid[1] = creal(tr); rp->braid[2] = cimag(tr);
  rp->count = rs->count;
  rc = digest(rs, rp->fingerprint, err);
  st_json_free(rs);
  return rc;
}

static double py_round10(double x) { char b[64]; snprintf(b, sizeof b, "%.10f", x); return strtod(b, NULL); }

st_code st_research_task_information(st_json **out, st_error *err) {
  st_ti_row *rows = NULL; const size_t n = 200;
  st_code rc = st_ti_generate(n, 20261117u, &rows, err);
  if (rc) return rc;
  size_t ntest = 100, hits[4] = {0};
  for (size_t i = 100; i < n && !rc; ++i) {
    st_ti_repr r; rc = st_ti_representation(rows[i].records, &r, err);
    if (rc) break;
    int pa = r.agnostic[0] > 0.023, pt = r.temporal > 0, pb = r.braid[0] > 0;
    hits[0] += pa == rows[i].label; hits[1] += pt == rows[i].label; hits[2] += pb == rows[i].label; hits[3] += pt == rows[i].label;
  }
  double acc[4]; for (int k = 0; k < 4; ++k) acc[k] = (double)hits[k] / (double)ntest;
  st_ti_repr r, dup, ch, rr; int dup_ok = 0, retained = 0, range_changes = 0;
  if (!rc) {
    const st_json *t0 = rows[100].records;
    st_json *d = st_json_clone(t0); st_json_push(d, st_json_clone(t0->items[0]));
    st_json *c = st_json_clone(t0), *changed = st_json_clone(t0->items[0]);
    st_json *ang = st_json_get(changed, "angles"); ang->items[4]->num += .05;
    st_json_push(c, changed);
    st_json *rg = st_json_array(), *rrec = st_json_clone(t0->items[0]); st_json_set(rrec, "range_m", st_json_double(1500.)); st_json_push(rg, rrec);
    rc = st_ti_representation(t0, &r, err);
    if (!rc) rc = st_ti_representation(d, &dup, err);
    if (!rc) rc = st_ti_representation(c, &ch, err);
    if (!rc) rc = st_ti_representation(rg, &rr, err);
    if (!rc) {
      dup_ok = !strcmp(r.fingerprint, dup.fingerprint) && r.braid[0] == dup.braid[0] && r.braid[1] == dup.braid[1] && r.braid[2] == dup.braid[2];
      retained = ch.count == 2;
      range_changes = strcmp(rr.fingerprint, r.fingerprint) != 0;
    }
    st_json_free(d); st_json_free(c); st_json_free(rg);
  }
  st_ti_free(rows, n);
  if (rc) return rc;
  st_json *o = st_json_object(), *m = st_json_object();
  st_json_set(o, "dataset", st_json_string("synthetic_angular_track_direction"));
  st_json_set(o, "seed", st_json_int(20261117));
  st_json_set(o, "train_n", st_json_int(100)); st_json_set(o, "test_n", st_json_int((int64_t)ntest));
  st_json_set(o, "task", st_json_string("binary_sign_of_underlying_angular_velocity"));
  st_json_set(o, "units", st_json_string("radians and seconds"));
  st_json_set(m, "lossy_absolute_magnitude_baseline", st_json_double(acc[0]));
  st_json_set(m, "temporal_signed_rate_baseline", st_json_double(acc[1]));
  st_json_set(m, "braid_abelianization", st_json_double(acc[2]));
  st_json_set(m, "combined_temporal_braid", st_json_double(acc[3]));
  st_json_set(o, "models", m);
  st_json_set(o, "duplication_invariant", st_json_bool(dup_ok));
  st_json_set(o, "different_measurement_retained", st_json_bool(retained));
  st_json_set(o, "range_field_changes_canonical_state", st_json_bool(range_changes));
  st_json_set(o, "range_without_platform_calibration_does_not_identify_metric_velocity", st_json_bool(1));
  st_json_set(o, "braid_vs_temporal_improvement", st_json_double(py_round10(acc[2] - acc[1])));
  st_json_set(o, "scope", st_json_string("No independent physics inference; braid crossing map is a modeling choice."));
  *out = o;
  return ST_E_OK;
}
