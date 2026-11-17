#include "skytrace/braid.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

st_code st_braid_features_from_track(const double *t, const double *x, const double *y, size_t n,
                                     const char *const *inputs, size_t n_inputs, st_feature_mode mode,
                                     st_braid_features *out, st_error *err) {
  memset(out, 0, sizeof *out);
  if (mode < ST_FEAT_WORD || mode > ST_FEAT_METRIC) return st_fail(err, ST_E_ARGUMENT, -1, "unknown feature mode");
  if (n_inputs == 0 || n_inputs > ST_FEAT_MAX_INPUTS) return st_fail(err, ST_E_SCHEMA, -1, "features must declare 1..%d originating sensor inputs", ST_FEAT_MAX_INPUTS);
  for (size_t i = 0; i < n_inputs; ++i) {
    if (!inputs[i] || !*inputs[i] || strlen(inputs[i]) >= sizeof out->inputs[0]) return st_fail(err, ST_E_SCHEMA, (long)i, "invalid declared input");
    strcpy(out->inputs[i], inputs[i]);
  }
  out->n_inputs = n_inputs; out->mode = mode;
  if (n < 3) return st_fail(err, ST_E_SHAPE, -1, "need >= 3 positions for one crossing");
  if (n - 2 > ST_BRAID_MAX_WORD) return st_fail(err, ST_E_BOUND, -1, "track too long");
  for (size_t i = 0; i < n; ++i) if (!isfinite(t[i]) || !isfinite(x[i]) || !isfinite(y[i])) return st_fail(err, ST_E_NONFINITE, (long)i, "non-finite track value");
  for (size_t i = 1; i < n; ++i) if (!(t[i] > t[i-1])) return st_fail(err, ST_E_TIME, (long)i, "track times must increase");
  double heading[ST_BRAID_MAX_WORD + 1], mid[ST_BRAID_MAX_WORD + 1], len[ST_BRAID_MAX_WORD + 1], raw_prev = 0;
  for (size_t i = 0; i + 1 < n; ++i) {
    double dx = x[i+1] - x[i], dy = y[i+1] - y[i], raw = atan2(dy, dx), h = raw;
    len[i] = hypot(dx, dy);
    if (!(len[i] > 0)) return st_fail(err, ST_E_SHAPE, (long)i, "zero-length displacement has no heading");
    if (i) { double d = raw - raw_prev, dd = fmod(d + PI, 2 * PI); if (dd < 0) dd += 2 * PI; dd -= PI;
             if (dd == -PI && d > 0) { dd = PI; }
             if (fabs(d) < PI) { dd = d; }
             h = heading[i-1] + dd; }
    heading[i] = h; raw_prev = raw; mid[i] = (t[i] + t[i+1]) / 2;
  }
  out->n_letters = n - 2;
  for (size_t k = 0; k + 2 < n; ++k) {
    double rate = (heading[k+1] - heading[k]) / (mid[k+1] - mid[k]);
    int gen = k % 2 == 0 ? 1 : 2;
    out->letters[k] = rate >= 0 ? gen : -gen;
    out->letter_time_s[k] = (mid[k] + mid[k+1]) / 2;
    out->turn_rate_rad_s[k] = fabs(rate);
    out->step_length_m[k] = len[k+1];
  }
  st_mat2 u;
  st_code rc = st_braid_word_matrix(out->letters, out->n_letters, &u, err);
  if (rc) return rc;
  if (st_mat2_unitarity_error(u) > 1e-10) return st_fail(err, ST_E_TOLERANCE, -1, "invalid braid unitary");
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) { out->fib_matrix[i*2+j] = creal(u.m[i][j]); out->fib_matrix[4+i*2+j] = cimag(u.m[i][j]); }
  return ST_E_OK;
}

static int same_features(const st_braid_features *a, const st_braid_features *b) {
  if (a->mode != b->mode) return 0;
  if (a->mode == ST_FEAT_FIB_MATRIX) { for (int i = 0; i < 8; ++i) if (fabs(a->fib_matrix[i] - b->fib_matrix[i]) > 1e-12) return 0; return 1; }
  if (a->n_letters != b->n_letters || memcmp(a->letters, b->letters, a->n_letters * sizeof *a->letters)) return 0;
  for (size_t k = 0; k < a->n_letters; ++k) {
    if (a->mode == ST_FEAT_WORD_TIMED && fabs(a->letter_time_s[k] - b->letter_time_s[k]) > 1e-12) return 0;
    if (a->mode == ST_FEAT_METRIC && (fabs(a->turn_rate_rad_s[k] - b->turn_rate_rad_s[k]) > 1e-12 ||
                                      fabs(a->step_length_m[k] - b->step_length_m[k]) > 1e-12)) return 0;
  }
  return 1;
}

int st_braid_collision(const st_braid_features *a, const st_braid_features *b, double physical_distance, double tol) {
  return physical_distance > tol && same_features(a, b);
}
