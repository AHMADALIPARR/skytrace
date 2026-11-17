// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Ridge regression exactly as the frozen research benchmarks define it (parameter_recovery
 * fit_ridge): column means and population standard deviations (sd < 1e-9 replaced by 1),
 * intercept column of ones left unpenalised, normal equations solved directly. NumPy's LAPACK
 * gesv is replaced by an explicit LU factorisation with partial pivoting. */
#include "skytrace/research.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const double ST_BUDGET_ALPHAS[5] = {0.01, 0.1, 1., 10., 100.};

static st_code lu_solve(double *M, double *rhs, size_t n, st_error *err) {
  for (size_t k = 0; k < n; ++k) {
    size_t piv = k; double best = fabs(M[k*n+k]);
    for (size_t i = k + 1; i < n; ++i) if (fabs(M[i*n+k]) > best) { best = fabs(M[i*n+k]); piv = i; }
    if (!(best > 0)) return st_fail(err, ST_E_SINGULAR, (long)k, "singular ridge normal equations");
    if (piv != k) {
      for (size_t j = 0; j < n; ++j) { double t = M[k*n+j]; M[k*n+j] = M[piv*n+j]; M[piv*n+j] = t; }
      double t = rhs[k]; rhs[k] = rhs[piv]; rhs[piv] = t;
    }
    for (size_t i = k + 1; i < n; ++i) {
      double f = M[i*n+k] / M[k*n+k];
      M[i*n+k] = f;
      for (size_t j = k + 1; j < n; ++j) M[i*n+j] -= f * M[k*n+j];
      rhs[i] -= f * rhs[k];
    }
  }
  for (size_t k = n; k-- > 0;) {
    double s = rhs[k];
    for (size_t j = k + 1; j < n; ++j) s -= M[k*n+j] * rhs[j];
    rhs[k] = s / M[k*n+k];
  }
  return ST_E_OK;
}

st_code st_ridge_fit(const double *X, const double *y, size_t n, size_t p, double alpha, st_ridge *m, st_error *err) {
  if (p > ST_RIDGE_MAX_P || n == 0) return st_fail(err, ST_E_SHAPE, -1, "ridge shape");
  memset(m, 0, sizeof *m); m->alpha = alpha; m->p = p;
  for (size_t j = 0; j < p; ++j) {          /* X.mean(axis=0): sequential over rows */
    double s = 0; for (size_t i = 0; i < n; ++i) s += X[i*p+j];
    m->mean[j] = s / (double)n;
  }
  for (size_t j = 0; j < p; ++j) {
    double s = 0; for (size_t i = 0; i < n; ++i) { double d = X[i*p+j] - m->mean[j]; s += d * d; }
    double sd = sqrt(s / (double)n);
    m->sd[j] = sd < 1e-9 ? 1. : sd;
  }
  size_t q = p + 1;
  double *A = malloc(n * q * sizeof *A), *G = calloc(q * q, sizeof *G), *r = calloc(q, sizeof *r);
  if (!A || !G || !r) { free(A); free(G); free(r); return st_fail(err, ST_E_NOMEM, -1, "ridge"); }
  for (size_t i = 0; i < n; ++i) { A[i*q] = 1.; for (size_t j = 0; j < p; ++j) A[i*q+1+j] = (X[i*p+j] - m->mean[j]) / m->sd[j]; }
  for (size_t a = 0; a < q; ++a) for (size_t b = 0; b < q; ++b) {
    double s = 0; for (size_t i = 0; i < n; ++i) s += A[i*q+a] * A[i*q+b];
    G[a*q+b] = s + (a == b && a > 0 ? alpha : 0.);
  }
  for (size_t a = 0; a < q; ++a) { double s = 0; for (size_t i = 0; i < n; ++i) s += A[i*q+a] * y[i]; r[a] = s; }
  st_code rc = lu_solve(G, r, q, err);
  if (!rc) memcpy(m->beta, r, q * sizeof *r);
  free(A); free(G); free(r);
  return rc;
}

double st_ridge_predict1(const st_ridge *m, const double *x) {
  double s = m->beta[0];
  for (size_t j = 0; j < m->p; ++j) s += ((x[j] - m->mean[j]) / m->sd[j]) * m->beta[1 + j];
  return s;
}

st_code st_ridge_select(const double *Xtr, const double *ytr, size_t ntr, const double *Xv, const double *yv, size_t nv,
                        size_t p, st_ridge *best, st_error *err) {
  double best_score = INFINITY; int have = 0;
  double *e = malloc(nv * sizeof *e);
  if (!e) return st_fail(err, ST_E_NOMEM, -1, "ridge select");
  for (int k = 0; k < 5; ++k) {
    st_ridge m; st_code rc = st_ridge_fit(Xtr, ytr, ntr, p, ST_BUDGET_ALPHAS[k], &m, err);
    if (rc) { free(e); return rc; }
    for (size_t i = 0; i < nv; ++i) { double d = st_ridge_predict1(&m, Xv + i * p) - yv[i]; e[i] = d * d; }
    double score = st_np_mean(e, nv, 1);
    if (!have || score < best_score) { best_score = score; *best = m; have = 1; }
  }
  free(e);
  return ST_E_OK;
}

st_reg_metrics st_reg_metrics_of(const double *truth, const double *pred, size_t n) {
  double *a = malloc(n * sizeof *a), *s = malloc(n * sizeof *s), *e = malloc(n * sizeof *e);
  st_reg_metrics m = {NAN, NAN, NAN};
  if (a && s && e) {
    for (size_t i = 0; i < n; ++i) { e[i] = pred[i] - truth[i]; a[i] = fabs(e[i]); s[i] = e[i] * e[i]; }
    m.mae = st_np_mean(a, n, 1); m.rmse = sqrt(st_np_mean(s, n, 1)); m.bias = st_np_mean(e, n, 1);
  }
  free(a); free(s); free(e);
  return m;
}
