// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* NumPy-compatible numerical kernels used by the research ports. Each follows the NumPy 2.x
 * definition of the same operation so that ported results agree with the Python reference to
 * rounding: np.add.reduce pairwise summation (8 accumulators, 128-element blocks), np.std with
 * ddof 0 (two passes), np.median, np.quantile(method='higher'), np.mod, np.unwrap, np.interp,
 * np.linspace, np.sinc, np.sign. */
#define _POSIX_C_SOURCE 200809L
#include "skytrace/research.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const double PI = 3.141592653589793;

double st_now_s(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec; }

double st_np_sum(const double *x, size_t n, size_t s) {
  if (n < 8) { double r = 0.; for (size_t i = 0; i < n; ++i) r += x[i * s]; return r; }
  if (n <= 128) {
    double r[8]; size_t i;
    for (int j = 0; j < 8; ++j) r[j] = x[(size_t)j * s];
    for (i = 8; i < n - (n % 8); i += 8) for (int j = 0; j < 8; ++j) r[j] += x[(i + (size_t)j) * s];
    double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) res += x[i * s];
    return res;
  }
  size_t n2 = n / 2; n2 -= n2 % 8;
  return st_np_sum(x, n2, s) + st_np_sum(x + n2 * s, n - n2, s);
}
double st_np_mean(const double *x, size_t n, size_t s) { return st_np_sum(x, n, s) / (double)n; }
double st_np_std(const double *x, size_t n, size_t s) {
  double m = st_np_mean(x, n, s), *d = malloc(n * sizeof *d);
  if (!d) return NAN;
  for (size_t i = 0; i < n; ++i) { double e = x[i * s] - m; d[i] = e * e; }
  double v = st_np_sum(d, n, 1) / (double)n; free(d);
  return sqrt(v);
}
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
double st_np_median(const double *x, size_t n) {
  double *c = malloc(n * sizeof *c); if (!c) return NAN;
  memcpy(c, x, n * sizeof *c); qsort(c, n, sizeof *c, cmpd);
  double r = n % 2 ? c[n / 2] : (0. + c[n / 2 - 1] + c[n / 2]) / 2.0;
  free(c); return r;
}
double st_np_quantile_higher(const double *x, size_t n, double q) {
  double *c = malloc(n * sizeof *c); if (!c) return NAN;
  memcpy(c, x, n * sizeof *c); qsort(c, n, sizeof *c, cmpd);
  /* _compute_virtual_index(n, q, alpha=1, beta=1) = n q + (1 + q (1 - 1 - 1)) - 1, then ceil */
  double vi = ceil((double)n * q + (1.0 + q * (1.0 - 1.0 - 1.0)) - 1.0);
  size_t k = vi < 0 ? 0 : vi > (double)(n - 1) ? n - 1 : (size_t)vi;
  double r = c[k]; free(c); return r;
}
double st_np_mod(double a, double b) {
  double m = fmod(a, b);
  if (m != 0) { if ((b < 0) != (m < 0)) m += b; }
  else m = copysign(0.0, b);
  return m;
}
void st_np_unwrap(const double *p, size_t n, double *out) {
  const double period = 2 * PI, high = period / 2, low = -high;
  double cum = 0;
  if (!n) return;
  out[0] = p[0];
  for (size_t i = 1; i < n; ++i) {
    double dd = p[i] - p[i - 1];
    double ddmod = st_np_mod(dd - low, period) + low;
    if (ddmod == low && dd > 0) ddmod = high;
    double corr = ddmod - dd;
    if (fabs(dd) < high) corr = 0;
    cum += corr;
    out[i] = p[i] + cum;
  }
}
double st_np_interp(double x, const double *xp, const double *fp, size_t n) {
  if (n == 1) return fp[0];
  if (x > xp[n - 1]) return fp[n - 1];
  if (x < xp[0]) return fp[0];
  size_t lo = 0, hi = n;                    /* largest j with xp[j] <= x */
  while (hi - lo > 1) { size_t mid = (lo + hi) / 2; if (xp[mid] <= x) lo = mid; else hi = mid; }
  size_t j = lo;
  if (j == n - 1 || xp[j] == x) return fp[j];
  double slope = (fp[j + 1] - fp[j]) / (xp[j + 1] - xp[j]);
  double r = slope * (x - xp[j]) + fp[j];
  if (isnan(r)) { r = slope * (x - xp[j + 1]) + fp[j + 1]; if (isnan(r) && fp[j] == fp[j + 1]) r = fp[j]; }
  return r;
}
void st_np_linspace(double a, double b, size_t n, double *out) {
  if (n == 1) { out[0] = a; return; }
  double step = (b - a) / (double)(n - 1);
  for (size_t i = 0; i < n; ++i) out[i] = (double)i * step + a;
  out[n - 1] = b;
}
double st_np_sinc(double x) { double y = PI * (x == 0 ? 1.0e-20 : x); return sin(y) / y; }
double st_np_sign(double x) { return x > 0 ? 1.0 : x < 0 ? -1.0 : x == 0 ? 0.0 : x; }
double st_polyfit_slope(const double *x, const double *y, size_t n) {
  double mx = st_np_mean(x, n, 1), my = st_np_mean(y, n, 1), sxx = 0, sxy = 0;
  for (size_t i = 0; i < n; ++i) { sxx += (x[i] - mx) * (x[i] - mx); sxy += (x[i] - mx) * (y[i] - my); }
  return sxy / sxx;
}

static void sv2(const double *A, size_t m, double *smax, double *smin) {
  double a = 0, b = 0, c = 0;
  for (size_t i = 0; i < m; ++i) { a += A[2*i] * A[2*i]; b += A[2*i] * A[2*i+1]; c += A[2*i+1] * A[2*i+1]; }
  double tr = a + c, disc = sqrt(fmax(0.0, (a - c) * (a - c) / 4 + b * b));
  double l1 = tr / 2 + disc, l2 = (a * c - b * b) / (l1 > 0 ? l1 : 1);
  *smax = sqrt(fmax(l1, 0)); *smin = sqrt(fmax(l2, 0));
}
int st_matrix_rank2(const double *A, size_t m) {
  double smax, smin; sv2(A, m, &smax, &smin);
  double tol = smax * (double)(m > 2 ? m : 2) * DBL_EPSILON;
  return (smax > tol) + (smin > tol);
}
st_code st_lstsq2(const double *A, const double *b, size_t m, double x[2], st_error *err) {
  if (m < 2) return st_fail(err, ST_E_SHAPE, -1, "lstsq needs >= 2 rows");
  if (st_matrix_rank2(A, m) < 2) return st_fail(err, ST_E_SINGULAR, -1, "rank-deficient triangulation");
  double *Q = malloc(m * 3 * sizeof *Q);
  if (!Q) return st_fail(err, ST_E_NOMEM, -1, "lstsq");
  for (size_t i = 0; i < m; ++i) { Q[3*i] = A[2*i]; Q[3*i+1] = A[2*i+1]; Q[3*i+2] = b[i]; }
  for (size_t k = 0; k < 2; ++k) {           /* Householder on column k, applied to the rest */
    double nrm = 0; for (size_t i = k; i < m; ++i) nrm += Q[3*i+k] * Q[3*i+k];
    nrm = sqrt(nrm);
    double alpha = Q[3*k+k] > 0 ? -nrm : nrm, v0 = Q[3*k+k] - alpha, vn = v0 * v0;
    for (size_t i = k + 1; i < m; ++i) vn += Q[3*i+k] * Q[3*i+k];
    if (vn > 0) for (size_t j = k + 1; j < 3; ++j) {
      double s = v0 * Q[3*k+j]; for (size_t i = k + 1; i < m; ++i) s += Q[3*i+k] * Q[3*i+j];
      s = 2 * s / vn;
      Q[3*k+j] -= s * v0; for (size_t i = k + 1; i < m; ++i) Q[3*i+j] -= s * Q[3*i+k];
    }
    Q[3*k+k] = alpha;
  }
  x[1] = Q[3*1+2] / Q[3*1+1];
  x[0] = (Q[2] - Q[1] * x[1]) / Q[0];
  free(Q);
  return ST_E_OK;
}
