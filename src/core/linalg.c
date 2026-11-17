#include "skytrace/core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ST_LINALG_MAX 64

st_code st_sym_eigen(double *a, size_t n, double *w, double *v, st_error *err) {
  if (n == 0 || n > ST_LINALG_MAX) return st_fail(err, ST_E_SHAPE, -1, "eigen size %zu out of range", n);
  for (size_t i = 0; i < n * n; ++i) if (!isfinite(a[i])) return st_fail(err, ST_E_NONFINITE, (long)i, "non-finite matrix entry");
  for (size_t i = 0; i < n; ++i) for (size_t j = i + 1; j < n; ++j)
    if (fabs(a[i*n+j] - a[j*n+i]) > 1e-12 * (1 + fabs(a[i*n+j]))) return st_fail(err, ST_E_SHAPE, -1, "matrix is not symmetric");
  double V[ST_LINALG_MAX * ST_LINALG_MAX];
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) V[i*n+j] = i == j;
  double fro = 0; for (size_t i = 0; i < n * n; ++i) fro += a[i] * a[i];
  fro = sqrt(fro);
  int converged = 0;
  for (int sweep = 0; sweep < 100; ++sweep) {
    double off = 0;
    for (size_t i = 0; i < n; ++i) for (size_t j = i + 1; j < n; ++j) off += 2 * a[i*n+j] * a[i*n+j];
    if (sqrt(off) <= 1e-15 * fro || off == 0) { converged = 1; break; }
    for (size_t p = 0; p < n; ++p) for (size_t q = p + 1; q < n; ++q) {
      double apq = a[p*n+q];
      if (apq == 0) continue;
      double theta = (a[q*n+q] - a[p*n+p]) / (2 * apq);
      double t = (theta >= 0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1));
      double c = 1 / sqrt(t * t + 1), s = t * c;
      for (size_t k = 0; k < n; ++k) {           /* columns p, q */
        double akp = a[k*n+p], akq = a[k*n+q];
        a[k*n+p] = c * akp - s * akq; a[k*n+q] = s * akp + c * akq;
      }
      for (size_t k = 0; k < n; ++k) {           /* rows p, q */
        double apk = a[p*n+k], aqk = a[q*n+k];
        a[p*n+k] = c * apk - s * aqk; a[q*n+k] = s * apk + c * aqk;
      }
      for (size_t k = 0; k < n; ++k) {
        double vkp = V[k*n+p], vkq = V[k*n+q];
        V[k*n+p] = c * vkp - s * vkq; V[k*n+q] = s * vkp + c * vkq;
      }
    }
  }
  if (!converged) return st_fail(err, ST_E_NOT_CONVERGED, -1, "Jacobi eigen did not converge");
  size_t idx[ST_LINALG_MAX];
  for (size_t i = 0; i < n; ++i) idx[i] = i;
  for (size_t i = 1; i < n; ++i) { size_t x = idx[i], j = i; while (j && a[x*n+x] < a[idx[j-1]*n+idx[j-1]]) { idx[j] = idx[j-1]; j--; } idx[j] = x; }
  for (size_t i = 0; i < n; ++i) {
    w[i] = a[idx[i]*n+idx[i]];
    if (v) for (size_t k = 0; k < n; ++k) v[k*n+i] = V[k*n+idx[i]];
  }
  return ST_E_OK;
}

static st_code cholesky(const double *a, size_t n, double *L, st_error *err) {
  if (n == 0 || n > ST_LINALG_MAX) return st_fail(err, ST_E_SHAPE, -1, "cholesky size out of range");
  memset(L, 0, n * n * sizeof *L);
  for (size_t j = 0; j < n; ++j) {
    double d = a[j*n+j];
    for (size_t k = 0; k < j; ++k) d -= L[j*n+k] * L[j*n+k];
    if (!(d > 0) || !isfinite(d)) return st_fail(err, ST_E_SINGULAR, (long)j, "matrix not positive definite at pivot %zu", j);
    L[j*n+j] = sqrt(d);
    for (size_t i = j + 1; i < n; ++i) {
      double s = a[i*n+j];
      for (size_t k = 0; k < j; ++k) s -= L[i*n+k] * L[j*n+k];
      L[i*n+j] = s / L[j*n+j];
    }
  }
  return ST_E_OK;
}
static void chol_apply(const double *L, size_t n, const double *b, double *x) {
  double y[ST_LINALG_MAX];
  for (size_t i = 0; i < n; ++i) { double s = b[i]; for (size_t k = 0; k < i; ++k) s -= L[i*n+k] * y[k]; y[i] = s / L[i*n+i]; }
  for (size_t i = n; i-- > 0;) { double s = y[i]; for (size_t k = i + 1; k < n; ++k) s -= L[k*n+i] * x[k]; x[i] = s / L[i*n+i]; }
}
st_code st_cholesky_solve(const double *a, size_t n, const double *b, double *x, st_error *err) {
  double L[ST_LINALG_MAX * ST_LINALG_MAX];
  st_code rc = cholesky(a, n, L, err);
  if (!rc) chol_apply(L, n, b, x);
  return rc;
}
st_code st_spd_inverse(const double *a, size_t n, double *inv, st_error *err) {
  double L[ST_LINALG_MAX * ST_LINALG_MAX], e[ST_LINALG_MAX], col[ST_LINALG_MAX];
  st_code rc = cholesky(a, n, L, err);
  if (rc) return rc;
  for (size_t j = 0; j < n; ++j) {
    for (size_t i = 0; i < n; ++i) e[i] = i == j;
    chol_apply(L, n, e, col);
    for (size_t i = 0; i < n; ++i) inv[i*n+j] = col[i];
  }
  return ST_E_OK;
}
