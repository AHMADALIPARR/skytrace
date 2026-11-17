// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* SKYTRACE 1.8 parameter-recovery benchmark core (port of research/parameter_recovery/benchmark.py):
 * synthetic constant-turn position tracks, canonical deduplication, the five feature branches,
 * the 1.9 full attributed graph vector and the direct constant-turn fit. Synthetic only. */
#include "skytrace/research.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.141592653589793;

/* ---------------------------------------------------------------- records */
static st_json *num_array(const double *v, size_t n) {
  st_json *a = st_json_array();
  for (size_t i = 0; a && i < n; ++i) st_json_push(a, st_json_double(v[i]));
  return a;
}
static st_json *make_record(int index, double sigma, const double *t, const double *x, const double *y, size_t n) {
  char origin[48]; snprintf(origin, sizeof origin, "origin-%d", index);
  st_json *r = st_json_object(), *pos = st_json_array();
  for (size_t i = 0; i < n; ++i) { double p[2] = {x[i], y[i]}; st_json_push(pos, num_array(p, 2)); }
  st_json_set(r, "origin", st_json_string(origin));
  st_json_set(r, "sensor", st_json_string("synthetic_position_meter_v1"));
  st_json_set(r, "calibration", st_json_string("wgs_local_cartesian_m_v1"));
  st_json_set(r, "sigma_m", st_json_double(sigma));
  st_json_set(r, "times_s", num_array(t, n));
  st_json_set(r, "positions_m", pos);
  return r;
}

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

st_code st_pr_simulate(int index, st_np_rng *g, size_t n, double sigma, double jitter, double drop, st_pr_row *row, st_error *err) {
  double omega = st_np_uniform(g, -.17, .17), v = st_np_uniform(g, 2.5, 8.), psi = st_np_uniform(g, -PI, PI);
  double x0 = st_np_uniform(g, -5, 5), y0 = st_np_uniform(g, -5, 5);
  double step = st_np_uniform(g, .35, .7);
  double *t = malloc(n * sizeof *t), *x = malloc(n * sizeof *x), *y = malloc(n * sizeof *y);
  int *keep = malloc(n * sizeof *keep);
  if (!t || !x || !y || !keep) { free(t); free(x); free(y); free(keep); return st_fail(err, ST_E_NOMEM, -1, "simulate"); }
  for (size_t i = 0; i < n; ++i) t[i] = (double)i * step;
  if (jitter != 0) {
    for (size_t i = 0; i < n; ++i) t[i] = t[i] + st_np_normal(g, 0, jitter);
    t[0] = 0; qsort(t, n, sizeof *t, cmpd);
  }
  for (size_t i = 0; i < n; ++i) {
    if (fabs(omega) < 1e-10) { x[i] = x0 + v * t[i] * cos(psi); y[i] = y0 + v * t[i] * sin(psi); }
    else {
      double k = v / omega;
      x[i] = x0 + k * (sin(psi + omega * t[i]) - sin(psi));
      y[i] = y0 - k * (cos(psi + omega * t[i]) - cos(psi));
    }
  }
  for (size_t i = 0; i < n; ++i) { x[i] = x[i] + st_np_normal(g, 0, sigma); y[i] = y[i] + st_np_normal(g, 0, sigma); }
  size_t m = n;
  if (drop != 0) {
    size_t kept = 0;
    for (size_t i = 0; i < n; ++i) keep[i] = st_np_random(g) >= drop;
    keep[0] = keep[n - 1] = 1;
    for (size_t i = 0; i < n; ++i) kept += (size_t)keep[i];
    if (kept < 5) for (size_t i = 0; i < 5 && i < n; ++i) keep[i] = 1;
    m = 0;
    for (size_t i = 0; i < n; ++i) if (keep[i]) { t[m] = t[i]; x[m] = x[i]; y[m] = y[i]; ++m; }
  }
  row->id = index; row->theta_rad_s = omega; row->speed_m_s = v;
  row->records = st_json_array();
  st_json_push(row->records, make_record(index, sigma, t, x, y, m));
  free(t); free(x); free(y); free(keep);
  return ST_E_OK;
}

void st_pr_row_free(st_pr_row *r) { st_json_free(r->records); r->records = NULL; }

st_code st_pr_generate(st_pr_data *d, st_error *err) {
  memset(d, 0, sizeof *d);
  const size_t n = 360;
  st_np_rng g; st_np_seed(&g, ST_PR_SEED);
  d->rows = calloc(n, sizeof *d->rows); d->order = malloc(n * sizeof *d->order);
  if (!d->rows || !d->order) return st_fail(err, ST_E_NOMEM, -1, "generate");
  d->n = n;
  for (size_t i = 0; i < n; ++i) { st_code rc = st_pr_simulate((int)i, &g, 18, 0.035, 0, 0, &d->rows[i], err); if (rc) return rc; }
  st_np_permutation(&g, n, d->order);
  d->ntrain = 220; d->nval = 70; d->ntest = 70;
  return ST_E_OK;
}
void st_pr_free(st_pr_data *d) {
  for (size_t i = 0; d->rows && i < d->n; ++i) st_pr_row_free(&d->rows[i]);
  free(d->rows); free(d->order); memset(d, 0, sizeof *d);
}
size_t st_pr_split_n(const st_pr_data *d, int w) { return w == 0 ? d->ntrain : w == 1 ? d->nval : d->ntest; }
const st_pr_row *st_pr_split(const st_pr_data *d, int w, size_t i) {
  size_t off = w == 0 ? 0 : w == 1 ? d->ntrain : d->ntrain + d->nval;
  return &d->rows[d->order[off + i]];
}

/* ---------------------------------------------------------------- canonicalisation */
st_code st_pr_record_digest(const st_json *rec, char hex[65], st_error *err) {
  st_buf b; st_buf_init(&b);
  if (!st_json_write(&b, rec, ST_JSON_PY_CANONICAL)) { st_buf_free(&b); return st_fail(err, ST_E_JSON, -1, "record not serialisable"); }
  st_sha256_hex(b.data, b.len, hex); st_buf_free(&b);
  return ST_E_OK;
}
typedef struct { char hex[65]; const st_json *rec; } keyed;
static int cmpk(const void *a, const void *b) { return strcmp(((const keyed *)a)->hex, ((const keyed *)b)->hex); }
st_code st_pr_canonicalize(const st_json *records, st_json **out, st_error *err) {
  if (!records || records->type != ST_JARR) return st_fail(err, ST_E_SCHEMA, -1, "records must be a list");
  keyed *k = calloc(records->count ? records->count : 1, sizeof *k); size_t nk = 0;
  if (!k) return st_fail(err, ST_E_NOMEM, -1, "canonicalize");
  for (size_t i = 0; i < records->count; ++i) {
    char hex[65]; st_code rc = st_pr_record_digest(records->items[i], hex, err);
    if (rc) { free(k); return rc; }
    size_t j = 0; while (j < nk && strcmp(k[j].hex, hex)) ++j;
    if (j == nk) { memcpy(k[nk].hex, hex, 65); nk++; }
    k[j].rec = records->items[i];           /* dict assignment: last value wins */
  }
  qsort(k, nk, sizeof *k, cmpk);
  *out = st_json_array();
  for (size_t i = 0; i < nk; ++i) st_json_push(*out, st_json_clone(k[i].rec));
  free(k);
  return ST_E_OK;
}

/* ---------------------------------------------------------------- measurements and derivatives */
typedef struct { size_t n; double *t, *x, *y; double sigma; } track;
static void track_free(track *t) { free(t->t); free(t->x); free(t->y); memset(t, 0, sizeof *t); }

static st_code measurements(const st_json *records, track *tk, st_error *err) {
  memset(tk, 0, sizeof *tk);
  st_json *rs = NULL; st_code rc = st_pr_canonicalize(records, &rs, err);
  if (rc) return rc;
  if (rs->count != 1) { st_json_free(rs); return st_fail(err, ST_E_INDEPENDENCE, -1, "baseline experiment requires exactly one unique sensor track"); }
  const st_json *rec = rs->items[0], *t = st_json_get(rec, "times_s"), *p = st_json_get(rec, "positions_m"), *s = st_json_get(rec, "sigma_m");
  if (!t || t->type != ST_JARR || !p || p->type != ST_JARR) { st_json_free(rs); return st_fail(err, ST_E_SCHEMA, -1, "invalid position track"); }
  size_t n = t->count;
  if (n < 5 || p->count != n) { st_json_free(rs); return st_fail(err, ST_E_TIME, -1, "invalid position track"); }
  tk->n = n; tk->t = malloc(n * sizeof(double)); tk->x = malloc(n * sizeof(double)); tk->y = malloc(n * sizeof(double));
  tk->sigma = s && s->type == ST_JNUM ? s->num : NAN;
  for (size_t i = 0; i < n; ++i) {
    const st_json *q = p->items[i];
    if (t->items[i]->type != ST_JNUM || q->type != ST_JARR || q->count != 2 || q->items[0]->type != ST_JNUM || q->items[1]->type != ST_JNUM) {
      track_free(tk); st_json_free(rs); return st_fail(err, ST_E_SCHEMA, (long)i, "invalid position track");
    }
    tk->t[i] = t->items[i]->num; tk->x[i] = q->items[0]->num; tk->y[i] = q->items[1]->num;
    if (i && !(tk->t[i] - tk->t[i-1] > 0)) { track_free(tk); st_json_free(rs); return st_fail(err, ST_E_TIME, (long)i, "invalid position track"); }
  }
  st_json_free(rs);
  return ST_E_OK;
}

typedef struct { track tk; size_t m; double *h, *mid, *r, *dt; } deriv;   /* m = n-1 headings, n-2 rates */
static void deriv_free(deriv *d) { track_free(&d->tk); free(d->h); free(d->mid); free(d->r); free(d->dt); }
static st_code derivatives(const st_json *records, deriv *d, st_error *err) {
  memset(d, 0, sizeof *d);
  st_code rc = measurements(records, &d->tk, err);
  if (rc) return rc;
  size_t n = d->tk.n, m = n - 1;
  d->m = m;
  double *raw = malloc(m * sizeof *raw);
  d->h = malloc(m * sizeof *d->h); d->mid = malloc(m * sizeof *d->mid); d->r = malloc(m * sizeof *d->r); d->dt = malloc(m * sizeof *d->dt);
  if (!raw || !d->h || !d->mid || !d->r || !d->dt) { free(raw); deriv_free(d); return st_fail(err, ST_E_NOMEM, -1, "derivatives"); }
  for (size_t i = 0; i < m; ++i) {
    raw[i] = atan2(d->tk.y[i+1] - d->tk.y[i], d->tk.x[i+1] - d->tk.x[i]);
    d->mid[i] = (d->tk.t[i+1] + d->tk.t[i]) / 2;
  }
  st_np_unwrap(raw, m, d->h); free(raw);
  for (size_t i = 0; i + 1 < m; ++i) { d->dt[i] = d->mid[i+1] - d->mid[i]; d->r[i] = (d->h[i+1] - d->h[i]) / d->dt[i]; }
  return ST_E_OK;
}

static size_t temporal(const deriv *d, double *f) {
  size_t n = d->tk.n, k = n - 2;
  double dts = st_np_sum(d->dt, k, 1), *w = malloc(k * sizeof *w), *len = malloc((n - 1) * sizeof *len);
  for (size_t i = 0; i < k; ++i) w[i] = (d->dt[i] / dts) * d->r[i];
  for (size_t i = 0; i + 1 < n; ++i) { double dx = d->tk.x[i+1] - d->tk.x[i], dy = d->tk.y[i+1] - d->tk.y[i]; len[i] = sqrt(dx * dx + dy * dy); }
  f[0] = st_polyfit_slope(d->mid, d->h, n - 1);
  f[1] = st_np_mean(d->r, k, 1);
  f[2] = st_np_sum(w, k, 1);
  f[3] = st_np_median(d->r, k);
  f[4] = st_np_std(d->r, k, 1);
  f[5] = d->h[n - 2] - d->h[0];
  f[6] = d->tk.t[n - 1] - d->tk.t[0];
  f[7] = (double)n;
  f[8] = st_np_mean(len, n - 1, 1);
  f[9] = st_np_median(d->dt, k);
  free(w); free(len);
  return 10;
}

st_code st_pr_braid_word(const st_json *records, int *word, size_t *n, size_t cap, st_error *err) {
  deriv d; st_code rc = derivatives(records, &d, err);
  if (rc) return rc;
  size_t k = d.tk.n - 2;
  if (k > cap) { deriv_free(&d); return st_fail(err, ST_E_BOUND, -1, "braid word too long"); }
  for (size_t i = 0; i < k; ++i) word[i] = (d.r[i] >= 0 ? 1 : -1) * (i % 2 == 0 ? 1 : 2);
  *n = k; deriv_free(&d);
  return ST_E_OK;
}

st_code st_pr_features(const st_json *records, st_pr_branch b, double *f, size_t *nf, st_error *err) {
  if (b == ST_PR_FULL_GRAPH) {
    track tk; st_code rc = measurements(records, &tk, err);
    if (rc) return rc;
    if (tk.n != 18) { track_free(&tk); return st_fail(err, ST_E_SHAPE, -1, "full graph fixed-grid fixture requires 18 nodes"); }
    size_t k = 0;
    for (size_t i = 0; i < 18; ++i) f[k++] = tk.t[i] - tk.t[0];
    for (size_t i = 0; i < 18; ++i) { f[k++] = tk.x[i] - tk.x[0]; f[k++] = tk.y[i] - tk.y[0]; }
    /* records[0]['sigma_m'] of the raw (not deduplicated) list, as in the Python branch */
    const st_json *s0 = st_json_get(records->items[0], "sigma_m");
    f[k++] = s0 && s0->type == ST_JNUM ? s0->num : NAN;
    *nf = k; track_free(&tk);
    return ST_E_OK;
  }
  deriv d; st_code rc = derivatives(records, &d, err);
  if (rc) return rc;
  size_t n = d.tk.n, k = n - 2;
  if (b == ST_PR_GRAPH) {
    double *dx = malloc((n - 1) * sizeof *dx), *dy = malloc((n - 1) * sizeof *dy);
    for (size_t i = 0; i + 1 < n; ++i) { dx[i] = d.tk.x[i+1] - d.tk.x[i]; dy[i] = d.tk.y[i+1] - d.tk.y[i]; }
    f[0] = (double)n; f[1] = d.tk.t[n-1] - d.tk.t[0];
    f[2] = st_np_mean(d.tk.x, n, 1); f[3] = st_np_mean(d.tk.y, n, 1);
    f[4] = st_np_std(d.tk.x, n, 1); f[5] = st_np_std(d.tk.y, n, 1);
    f[6] = st_np_mean(dx, n - 1, 1); f[7] = st_np_mean(dy, n - 1, 1);
    f[8] = st_np_std(dx, n - 1, 1); f[9] = st_np_std(dy, n - 1, 1);
    free(dx); free(dy); *nf = 10; deriv_free(&d);
    return ST_E_OK;
  }
  if (b == ST_PR_TEMPORAL) { *nf = temporal(&d, f); deriv_free(&d); return ST_E_OK; }
  /* braid word features */
  double *s = malloc(k * sizeof *s), *pr = malloc(k * sizeof *pr), *tr = malloc(k * sizeof *tr), *sd = malloc(k * sizeof *sd);
  int c1 = 0, cm1 = 0, c2 = 0, cm2 = 0;
  for (size_t i = 0; i < k; ++i) {
    int w = (d.r[i] >= 0 ? 1 : -1) * (i % 2 == 0 ? 1 : 2);
    s[i] = w > 0 ? 1. : -1.;
    c1 += w == 1; cm1 += w == -1; c2 += w == 2; cm2 += w == -2;
  }
  for (size_t i = 0; i + 1 < k; ++i) { pr[i] = s[i] * s[i+1]; tr[i] = (s[i+1] - s[i]) != 0 ? 1. : 0.; }
  f[0] = (double)k; f[1] = st_np_sum(s, k, 1); f[2] = st_np_mean(s, k, 1);
  f[3] = st_np_mean(pr, k - 1, 1); f[4] = st_np_mean(tr, k - 1, 1);
  f[5] = c1; f[6] = cm1; f[7] = c2; f[8] = cm2;
  size_t m = 9;
  if (b == ST_PR_BRAID_TIMED) {
    for (size_t i = 0; i < k; ++i) sd[i] = s[i] * d.dt[i];
    double sdt = st_np_sum(sd, k, 1);
    f[m++] = sdt / st_np_sum(d.dt, k, 1);
    f[m++] = sdt;
    f[m++] = st_np_mean(d.dt, k, 1);
    f[m++] = st_np_std(d.dt, k, 1);
    f[m++] = d.tk.t[n-1] - d.tk.t[0];
  } else if (b == ST_PR_BRAID_METRIC) {
    m += temporal(&d, f + m);
  } else if (b != ST_PR_BRAID_WORD) {
    free(s); free(pr); free(tr); free(sd); deriv_free(&d);
    return st_fail(err, ST_E_ARGUMENT, (long)b, "unknown branch");
  }
  free(s); free(pr); free(tr); free(sd); deriv_free(&d);
  *nf = m;
  return ST_E_OK;
}

/* ---------------------------------------------------------------- direct constant-turn fit */
/* Model of the Python reference: d = v t sinc(w t / 2 pi) [cos(psi + w t/2), sin(psi + w t/2)],
 * |w| < 1e-8 switches to the straight line. Residuals (pred - xy) interleaved x, y. */
static void dsinc_du(double u, double *s, double *ds) {
  if (fabs(u) < 1e-4) { double u2 = u * u; *s = 1 - u2 / 6 + u2 * u2 / 120; *ds = -u / 3 + u * u2 / 30; }
  else { *s = sin(u) / u; *ds = (u * cos(u) - sin(u)) / (u * u); }
}
static void direct_model(const double *p, const track *tk, double *r, double *J) {
  double x0 = p[0], y0 = p[1], v = p[2], psi = p[3], w = p[4];
  for (size_t i = 0; i < tk->n; ++i) {
    double t = tk->t[i], dx, dy, jx[5], jy[5];
    if (fabs(w) < 1e-8) {
      dx = v * t * cos(psi); dy = v * t * sin(psi);
      jx[2] = t * cos(psi); jy[2] = t * sin(psi); jx[3] = -dy; jy[3] = dx;
      jx[4] = -v * t * t / 2 * sin(psi); jy[4] = v * t * t / 2 * cos(psi);
    } else {
      double sc = st_np_sinc((w * t) / (2 * PI)), u = w * t / 2, s, ds, ph = psi + w * t / 2;
      dsinc_du(u, &s, &ds);
      dx = v * sc * t * cos(ph); dy = v * sc * t * sin(ph);
      jx[2] = sc * t * cos(ph); jy[2] = sc * t * sin(ph);
      jx[3] = -dy; jy[3] = dx;
      jx[4] = v * t * ds * (t / 2) * cos(ph) - dy * (t / 2);
      jy[4] = v * t * ds * (t / 2) * sin(ph) + dx * (t / 2);
    }
    r[2*i] = x0 + dx - tk->x[i]; r[2*i+1] = y0 + dy - tk->y[i];
    if (J) {
      jx[0] = 1; jx[1] = 0; jy[0] = 0; jy[1] = 1;
      memcpy(J + (2*i) * 5, jx, sizeof jx); memcpy(J + (2*i+1) * 5, jy, sizeof jy);
    }
  }
}

static st_code solve5(double *A, double *b, size_t n) {   /* Gaussian elimination with partial pivoting */
  for (size_t k = 0; k < n; ++k) {
    size_t p = k; for (size_t i = k + 1; i < n; ++i) if (fabs(A[i*n+k]) > fabs(A[p*n+k])) p = i;
    if (!(fabs(A[p*n+k]) > 0)) return ST_E_SINGULAR;
    if (p != k) { for (size_t j = 0; j < n; ++j) { double t = A[k*n+j]; A[k*n+j] = A[p*n+j]; A[p*n+j] = t; } double t = b[k]; b[k] = b[p]; b[p] = t; }
    for (size_t i = k + 1; i < n; ++i) { double f = A[i*n+k] / A[k*n+k]; for (size_t j = k; j < n; ++j) A[i*n+j] -= f * A[k*n+j]; b[i] -= f * b[k]; }
  }
  for (size_t k = n; k-- > 0;) { double s = b[k]; for (size_t j = k + 1; j < n; ++j) s -= A[k*n+j] * b[j]; b[k] = s / A[k*n+k]; }
  return ST_E_OK;
}

st_code st_pr_direct_fit(const st_json *records, double *omega, st_error *err) {
  track tk; st_code rc = measurements(records, &tk, err);
  if (rc) return rc;
  size_t n = tk.n, m = 2 * n;
  double *vel = malloc((n - 1) * sizeof *vel);
  for (size_t i = 0; i + 1 < n; ++i) {
    double dt = tk.t[i+1] - tk.t[i], vx = (tk.x[i+1] - tk.x[i]) / dt, vy = (tk.y[i+1] - tk.y[i]) / dt;
    vel[i] = sqrt(vx * vx + vy * vy);
  }
  double v0 = st_np_median(vel, n - 1);
  double dt0 = tk.t[1] - tk.t[0], angle = atan2((tk.y[1] - tk.y[0]) / dt0, (tk.x[1] - tk.x[0]) / dt0);
  free(vel);
  double tf[ST_PR_MAX_FEATURES]; size_t nf = 0;
  if ((rc = st_pr_features(records, ST_PR_TEMPORAL, tf, &nf, err))) { track_free(&tk); return rc; }
  double p[5] = {tk.x[0], tk.y[0], v0, angle, tf[0]};
  const double lb[5] = {-INFINITY, -INFINITY, .1, -INFINITY, -.5}, ub[5] = {INFINITY, INFINITY, 30., INFINITY, .5};
  for (int j = 0; j < 5; ++j) p[j] = fmin(fmax(p[j], lb[j]), ub[j]);
  double *r = malloc(m * sizeof *r), *J = malloc(m * 5 * sizeof *J), *rt = malloc(m * sizeof *rt);
  direct_model(p, &tk, r, J);
  double cost = 0; for (size_t i = 0; i < m; ++i) cost += r[i] * r[i];
  double lambda = 1e-3;
  for (int it = 0; it < 500; ++it) {
    double G[25] = {0}, g[5] = {0};
    for (size_t i = 0; i < m; ++i) for (int a = 0; a < 5; ++a) { g[a] += J[i*5+a] * r[i]; for (int b = 0; b < 5; ++b) G[a*5+b] += J[i*5+a] * J[i*5+b]; }
    int accepted = 0;
    for (int tries = 0; tries < 60 && !accepted; ++tries) {
      double A[25], step[5], q[5];
      memcpy(A, G, sizeof A);
      for (int a = 0; a < 5; ++a) { A[a*5+a] += lambda * (G[a*5+a] > 0 ? G[a*5+a] : 1); step[a] = -g[a]; }
      if (solve5(A, step, 5)) { lambda *= 10; continue; }
      for (int a = 0; a < 5; ++a) q[a] = fmin(fmax(p[a] + step[a], lb[a]), ub[a]);
      direct_model(q, &tk, rt, NULL);
      double c2 = 0; for (size_t i = 0; i < m; ++i) c2 += rt[i] * rt[i];
      if (c2 <= cost) {
        double dx = 0, xn = 0; for (int a = 0; a < 5; ++a) { dx += (q[a] - p[a]) * (q[a] - p[a]); xn += p[a] * p[a]; }
        double dc = cost - c2;
        memcpy(p, q, sizeof p); cost = c2; lambda = fmax(lambda / 10, 1e-12); accepted = 1;
        direct_model(p, &tk, r, J);
        if (sqrt(dx) <= 1e-14 * (1e-14 + sqrt(xn)) || dc <= 1e-16 * cost) it = 1 << 20;
      } else lambda *= 10;
    }
    if (!accepted) break;
  }
  *omega = p[4];
  free(r); free(J); free(rt); track_free(&tk);
  return ST_E_OK;
}

/* ---------------------------------------------------------------- braid-word collision control */
st_code st_pr_collision_example(st_pr_collision *c, st_error *err) {
  st_np_rng g; st_np_seed(&g, 91);
  st_pr_row a = {0}; st_code rc = st_pr_simulate(10, &g, 24, 0., 0, 0, &a, err);
  if (rc) return rc;
  const st_json *rec = a.records->items[0], *ts = st_json_get(rec, "times_s"), *p0 = st_json_get(rec, "positions_m")->items[0];
  double x0 = p0->items[0]->num, y0 = p0->items[1]->num, psi = 0.3, v = 4.;
  st_json *bc[2]; const double om[2] = {.12, .04};
  for (int k = 0; k < 2; ++k) {
    bc[k] = st_json_clone(a.records);
    st_json *pos = st_json_array();
    for (size_t i = 0; i < ts->count; ++i) {
      double ti = ts->items[i]->num, w = om[k];
      st_json *pt = st_json_array();
      st_json_push(pt, st_json_double(x0 + v / w * (sin(psi + w * ti) - sin(psi))));
      st_json_push(pt, st_json_double(y0 - v / w * (cos(psi + w * ti) - cos(psi))));
      st_json_push(pos, pt);
    }
    st_json_set(bc[k]->items[0], "positions_m", pos);
  }
  int w1[64], w2[64]; size_t n1, n2; double f1[16], f2[16]; size_t m1, m2;
  if ((rc = st_pr_braid_word(bc[0], w1, &n1, 64, err)) || (rc = st_pr_braid_word(bc[1], w2, &n2, 64, err)) ||
      (rc = st_pr_features(bc[0], ST_PR_TEMPORAL, f1, &m1, err)) || (rc = st_pr_features(bc[1], ST_PR_TEMPORAL, f2, &m2, err))) {
    st_json_free(bc[0]); st_json_free(bc[1]); st_pr_row_free(&a); return rc;
  }
  c->same_braid_word = n1 == n2 && !memcmp(w1, w2, n1 * sizeof *w1);
  int close = m1 == m2;
  for (size_t i = 0; close && i < m1; ++i) close = fabs(f1[i] - f2[i]) <= 1e-8 + 1e-5 * fabs(f2[i]);
  c->same_temporal_features = close;
  st_json_free(bc[0]); st_json_free(bc[1]); st_pr_row_free(&a);
  return ST_E_OK;
}
