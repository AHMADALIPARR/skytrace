#include "skytrace/braid.h"
#include "check.h"

static st_braid_features fa, fb;

static double ref_err(const st_json *ref, const char *key, st_mat2 m) {
  const st_json *a = st_json_get(ref, key); double e = 0;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) {
    const st_json *z = a->items[i]->items[j];
    double d = cabs(m.m[i][j] - (z->items[0]->num + I * z->items[1]->num)); if (d > e) e = d;
  }
  return e;
}
static int equiv(const int *a, size_t na, const int *b, size_t nb) { int eq = -1; st_error e; if (st_braid_equivalent(a, na, b, nb, &eq, &e)) return -1; return eq; }
static uint32_t rng = 88172645u;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main(int argc, char **argv) {
  const char *src = argc > 1 ? argv[1] : ".";
  st_error e; st_mat2 s1, s2, m; st_fib_generators(&s1, &s2);
  /* BF-01 and the contract tolerance 1e-12 */
  st_braid_checks c; st_braid_run_checks(&c);
  CHECK(c.unitarity_s1 < 1e-12 && c.unitarity_s2 < 1e-12 && c.braid_relation < 1e-12);
  CHECK(c.inverse_s1 < 1e-12 && c.inverse_s2 < 1e-12 && c.F_involution < 1e-12 && c.golden_identity < 1e-12);
  int w121[] = {1, 2, 1}, w212[] = {2, 1, 2}, inv[] = {1, -1, 2, -2}, bad[] = {3};
  st_mat2 a, b; st_braid_word_matrix(w121, 3, &a, &e); st_braid_word_matrix(w212, 3, &b, &e);
  CHECK(st_mat2_max_error(a, b) < 1e-12);
  CHECK(st_braid_word_matrix(inv, 4, &m, &e) == ST_E_OK && st_mat2_max_error(m, st_mat2_identity()) < 1e-12);
  CHECK(st_braid_word_matrix(bad, 1, &m, &e) == ST_E_ARGUMENT);
  /* Agreement with matrices produced by research/speculative_math/core.py */
  char path[4200]; snprintf(path, sizeof path, "%s/fixtures/reference/fibonacci_braid.reference.json", src);
  st_json *ref = st_json_parse_file(path, &e); CHECK(ref != NULL);
  if (ref) {
    int w5[] = {1, 2, 2, -1, 2};
    CHECK(ref_err(ref, "sigma1", s1) < 1e-15); CHECK(ref_err(ref, "sigma2", s2) < 1e-15);
    CHECK(ref_err(ref, "word_121", a) < 1e-15); CHECK(ref_err(ref, "word_212", b) < 1e-15);
    st_braid_word_matrix(w5, 5, &m, &e); CHECK(ref_err(ref, "word_1_2_2_m1_2", m) < 1e-14);
    st_json_free(ref);
  }
  /* Fusion rules tau x tau = 1 + tau; fusion-space dimensions are Fibonacci numbers. */
  CHECK(st_fib_fusion(1, 1, 0) == 1 && st_fib_fusion(1, 1, 1) == 1 && st_fib_fusion(0, 1, 1) == 1 && st_fib_fusion(0, 1, 0) == 0 && st_fib_fusion(0, 0, 0) == 1);
  uint64_t d, fib[40] = {0, 1};
  for (int i = 2; i < 40; ++i) fib[i] = fib[i-1] + fib[i-2];
  for (unsigned n = 1; n < 30; ++n) { CHECK(st_fib_fusion_space_dim(n, 1, &d, &e) == ST_E_OK && d == fib[n]);
                                      CHECK(st_fib_fusion_space_dim(n, 0, &d, &e) == ST_E_OK && d == fib[n-1]); }
  CHECK(st_fib_fusion_space_dim(100, 1, &d, &e) == ST_E_OVERFLOW);
  double phi = st_golden_ratio(); CHECK_NEAR(phi * phi, 1 + phi, 1e-15);   /* quantum dimension d^2 = 1 + d */
  /* Exact word problem (Burau, faithful on B3) */
  int w12[] = {1, 2}, w21[] = {2, 1}, e0[] = {0}, cancel[] = {1, -1}, d2a[] = {1, 2, 1, 1, 2, 1, 1}, d2b[] = {1, 1, 2, 1, 1, 2, 1};
  CHECK(equiv(w121, 3, w212, 3) == 1);
  CHECK(equiv(w12, 2, w21, 2) == 0);
  CHECK(equiv(cancel, 2, e0, 0) == 1);
  CHECK(equiv(d2a, 7, d2b, 7) == 1);                 /* Delta^2 is central: Delta^2 s1 = s1 Delta^2 */
  int s1cube[] = {1, 1, 1}, s2cube[] = {2, 2, 2}; CHECK(equiv(s1cube, 3, s2cube, 3) == 0);
  /* Property tests: w w^-1 = e; relation rewrites preserve both representations. */
  static int w[600], wi[600], w2[600];
  int fails = 0, fib_fail = 0;
  for (int trial = 0; trial < 300; ++trial) {
    size_t n = 1 + next() % 40;
    for (size_t i = 0; i < n; ++i) { int g = 1 + (int)(next() % 2); w[i] = (next() % 2) ? g : -g; }
    for (size_t i = 0; i < n; ++i) wi[i] = -w[n - 1 - i];
    memcpy(w2, w, n * sizeof *w); memcpy(w2 + n, wi, n * sizeof *w);
    if (equiv(w2, 2 * n, e0, 0) != 1) fails++;
    /* insert the relation s1 s2 s1 (s2 s1 s2)^-1 at a random position: same braid */
    size_t at = next() % (n + 1), k = 0; static int ins[] = {1, 2, 1, -2, -1, -2};
    for (size_t i = 0; i < at; ++i) w2[k++] = w[i];
    for (int i = 0; i < 6; ++i) w2[k++] = ins[i];
    for (size_t i = at; i < n; ++i) w2[k++] = w[i];
    if (equiv(w, n, w2, k) != 1) fails++;
    st_mat2 ma, mb; st_braid_word_matrix(w, n, &ma, &e); st_braid_word_matrix(w2, k, &mb, &e);
    if (st_mat2_max_error(ma, mb) > 1e-10) fib_fail++;
    size_t r = st_braid_free_reduce(memcpy(w2, w, n * sizeof *w), n);
    if (equiv(w, n, w2, r) != 1) fails++;
  }
  CHECK(fails == 0); CHECK(fib_fail == 0);
  int red[] = {1, 2, -2, -1, 2}; CHECK(st_braid_free_reduce(red, 5) == 1 && red[0] == 2);
  /* Long pseudo-Anosov words overflow int64 Burau coefficients: reported, never wrapped. */
  static int pa[4000]; for (int i = 0; i < 4000; ++i) pa[i] = i % 2 ? -2 : 1;
  int eq; CHECK(st_braid_equivalent(pa, 4000, e0, 0, &eq, &e) == ST_E_OVERFLOW);
  static int toolong[ST_BRAID_MAX_WORD + 1]; for (int i = 0; i <= ST_BRAID_MAX_WORD; ++i) toolong[i] = 1;
  CHECK(st_braid_word_matrix(toolong, ST_BRAID_MAX_WORD + 1, &m, &e) == ST_E_BOUND);

  /* Feature modes and collision detection: different turn rates, same braid word. */
  double t[30], xa[30], ya[30], xb[30], yb[30];
  for (int i = 0; i < 30; ++i) { t[i] = 0.2 * i; xa[i] = 50 * sin(0.05 * t[i]); ya[i] = 50 * (1 - cos(0.05 * t[i]));
                                 xb[i] = 25 * sin(0.10 * t[i]); yb[i] = 25 * (1 - cos(0.10 * t[i])); }
  const char *inputs[] = {"synthetic-camera-1", "sha256:4e9c5ad981c0afe814545fe46a634c70b4f3a02cf5f97bf4e86fba31d8e4075a"};
  CHECK(st_braid_features_from_track(t, xa, ya, 30, inputs, 0, ST_FEAT_WORD, &fa, &e) == ST_E_SCHEMA);   /* inputs must be declared */
  CHECK(st_braid_features_from_track(t, xa, ya, 30, inputs, 2, ST_FEAT_WORD, &fa, &e) == ST_E_OK);
  CHECK(st_braid_features_from_track(t, xb, yb, 30, inputs, 2, ST_FEAT_WORD, &fb, &e) == ST_E_OK);
  CHECK(fa.n_letters == 28 && fa.n_inputs == 2 && !strcmp(fa.inputs[0], "synthetic-camera-1"));
  CHECK(fa.letters[0] == 1 && fa.letters[1] == 2);
  CHECK(st_braid_collision(&fa, &fb, 0.05, 1e-9) == 1);                /* word-only features collide */
  fa.mode = fb.mode = ST_FEAT_WORD_TIMED; CHECK(st_braid_collision(&fa, &fb, 0.05, 1e-9) == 1);  /* same sampling times */
  fa.mode = fb.mode = ST_FEAT_FIB_MATRIX; CHECK(st_braid_collision(&fa, &fb, 0.05, 1e-9) == 1);  /* matrix of same word */
  fa.mode = fb.mode = ST_FEAT_METRIC; CHECK(st_braid_collision(&fa, &fb, 0.05, 1e-9) == 0);      /* metric attributes separate them */
  CHECK_NEAR(fa.turn_rate_rad_s[3], 0.05, 1e-9); CHECK_NEAR(fb.turn_rate_rad_s[3], 0.10, 1e-9);
  CHECK(st_braid_collision(&fa, &fa, 0.0, 1e-9) == 0);                 /* identical physics is not a collision */
  for (int i = 0; i < 30; ++i) ya[i] = -ya[i];                          /* mirror: right turns give inverse letters */
  CHECK(st_braid_features_from_track(t, xa, ya, 30, inputs, 1, ST_FEAT_WORD, &fa, &e) == ST_E_OK && fa.letters[0] == -1 && fa.letters[1] == -2);
  t[5] = t[4]; CHECK(st_braid_features_from_track(t, xa, ya, 30, inputs, 1, ST_FEAT_WORD, &fa, &e) == ST_E_TIME);
  return finish("test_braid");
}
