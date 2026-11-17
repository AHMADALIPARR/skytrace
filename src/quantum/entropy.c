// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/entropy.h"
#include <math.h>

st_code st_hermitian_eigenvalues(const st_cmat *h, double *w, st_error *err) {
  size_t n = h->n, m = 2 * n;
  if (n == 0 || n > ST_Q_MAX_DIM) return st_fail(err, ST_E_SHAPE, -1, "dimension out of range");
  double dev = st_cmat_hermitian_deviation(h);
  if (dev > ST_Q_TOL) return st_fail(err, ST_E_NOT_HERMITIAN, -1, "hermitian deviation %.3g", dev);
  double S[4 * ST_Q_MAX_DIM * ST_Q_MAX_DIM], ww[2 * ST_Q_MAX_DIM];
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) {
    /* symmetrise exactly so the Jacobi symmetry precondition holds bit-for-bit */
    double x = (creal(h->a[i*n+j]) + creal(h->a[j*n+i])) / 2, y = (cimag(h->a[i*n+j]) - cimag(h->a[j*n+i])) / 2;
    S[i*m+j] = x; S[(i+n)*m+(j+n)] = x; S[i*m+(j+n)] = -y; S[(i+n)*m+j] = y;
  }
  st_code rc = st_sym_eigen(S, m, ww, NULL, err);
  if (rc) return rc;
  for (size_t k = 0; k < n; ++k) w[k] = (ww[2*k] + ww[2*k+1]) / 2;
  return ST_E_OK;
}

st_code st_von_neumann_entropy_bits(const st_cmat *rho, double tol, double *S, st_error *err) {
  double w[ST_Q_MAX_DIM]; st_code rc = st_hermitian_eigenvalues(rho, w, err);
  if (rc) return rc;
  double s = 0;
  for (size_t k = 0; k < rho->n; ++k) {
    if (w[k] < -tol) return st_fail(err, ST_E_NOT_POSITIVE, (long)k, "negative eigenvalue %.3g", w[k]);
    if (w[k] > tol) s -= w[k] * log2(w[k]);
  }
  *S = s == 0 ? 0.0 : s; return ST_E_OK;
}
