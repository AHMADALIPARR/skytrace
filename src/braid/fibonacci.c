#include "skytrace/braid.h"
#include <math.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

st_mat2 st_mat2_identity(void) { st_mat2 r = {{{1, 0}, {0, 1}}}; return r; }
st_mat2 st_mat2_mul(st_mat2 a, st_mat2 b) {
  st_mat2 r;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j];
  return r;
}
st_mat2 st_mat2_dagger(st_mat2 a) {
  st_mat2 r;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) r.m[i][j] = conj(a.m[j][i]);
  return r;
}
double st_mat2_max_error(st_mat2 a, st_mat2 b) {
  double e = 0;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) { double d = cabs(a.m[i][j] - b.m[i][j]); if (d > e) e = d; }
  return e;
}
double st_mat2_unitarity_error(st_mat2 a) { return st_mat2_max_error(st_mat2_mul(a, st_mat2_dagger(a)), st_mat2_identity()); }

double st_golden_ratio(void) { return (1 + sqrt(5.0)) / 2; }
st_mat2 st_fib_F(void) {
  double phi = st_golden_ratio(), a = 1 / phi, b = 1 / sqrt(phi);
  st_mat2 f = {{{a, b}, {b, -a}}}; return f;
}
st_mat2 st_fib_R(void) {
  st_mat2 r = {{{cexp(-4 * PI * I / 5), 0}, {0, cexp(3 * PI * I / 5)}}}; return r;
}
void st_fib_generators(st_mat2 *s1, st_mat2 *s2) {
  st_mat2 F = st_fib_F(), R = st_fib_R();
  *s1 = R; *s2 = st_mat2_mul(st_mat2_mul(F, R), F);
}

int st_fib_fusion(int a, int b, int c) {
  if (a < 0 || a > 1 || b < 0 || b > 1 || c < 0 || c > 1) return 0;
  if (a == 0) return b == c;          /* 1 x b = b */
  if (b == 0) return a == c;
  return 1;                           /* tau x tau = 1 + tau */
}

st_code st_fib_fusion_space_dim(unsigned n, int total, uint64_t *dim, st_error *err) {
  if (n == 0 || (total != 0 && total != 1)) return st_fail(err, ST_E_ARGUMENT, -1, "need n >= 1 and total in {0,1}");
  uint64_t cnt[2] = {0, 1};           /* one tau: charge tau with multiplicity 1 */
  for (unsigned k = 1; k < n; ++k) {
    uint64_t next[2] = {0, 0};
    for (int a = 0; a < 2; ++a) for (int c = 0; c < 2; ++c) if (st_fib_fusion(a, 1, c)) {
      if (next[c] > UINT64_MAX - cnt[a]) return st_fail(err, ST_E_OVERFLOW, (long)k, "fusion space dimension overflows uint64");
      next[c] += cnt[a];
    }
    cnt[0] = next[0]; cnt[1] = next[1];
  }
  *dim = cnt[total]; return ST_E_OK;
}

st_code st_braid_word_matrix(const int *w, size_t n, st_mat2 *out, st_error *err) {
  if (n > ST_BRAID_MAX_WORD) return st_fail(err, ST_E_BOUND, -1, "braid word longer than %d", ST_BRAID_MAX_WORD);
  st_mat2 s1, s2; st_fib_generators(&s1, &s2);
  st_mat2 d1 = st_mat2_dagger(s1), d2 = st_mat2_dagger(s2), u = st_mat2_identity();
  for (size_t i = 0; i < n; ++i) {
    switch (w[i]) {
      case 1: u = st_mat2_mul(u, s1); break; case 2: u = st_mat2_mul(u, s2); break;
      case -1: u = st_mat2_mul(u, d1); break; case -2: u = st_mat2_mul(u, d2); break;
      default: return st_fail(err, ST_E_ARGUMENT, (long)i, "braid generator must be +1,+2,-1,-2");
    }
  }
  *out = u; return ST_E_OK;
}

size_t st_braid_free_reduce(int *w, size_t n) {
  size_t k = 0;
  for (size_t i = 0; i < n; ++i) { if (k && w[k-1] == -w[i]) k--; else w[k++] = w[i]; }
  return k;
}

void st_braid_run_checks(st_braid_checks *c) {
  st_mat2 s1, s2, F = st_fib_F(), R = st_fib_R(); st_fib_generators(&s1, &s2);
  st_mat2 I2 = st_mat2_identity();
  c->unitarity_s1 = st_mat2_unitarity_error(s1);
  c->unitarity_s2 = st_mat2_unitarity_error(s2);
  c->braid_relation = st_mat2_max_error(st_mat2_mul(st_mat2_mul(s1, s2), s1), st_mat2_mul(st_mat2_mul(s2, s1), s2));
  c->inverse_s1 = st_mat2_max_error(st_mat2_mul(s1, st_mat2_dagger(s1)), I2);
  c->inverse_s2 = st_mat2_max_error(st_mat2_mul(s2, st_mat2_dagger(s2)), I2);
  c->F_involution = st_mat2_max_error(st_mat2_mul(F, F), I2);
  c->F_unitarity = st_mat2_unitarity_error(F);
  c->R_unitarity = st_mat2_unitarity_error(R);
  double phi = st_golden_ratio(); c->golden_identity = fabs(phi * phi - phi - 1);
}
