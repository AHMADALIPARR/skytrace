#ifndef SKYTRACE_BRAID_H
#define SKYTRACE_BRAID_H
/* Artin braid group B_3 = < s1, s2 | s1 s2 s1 = s2 s1 s2 > and its Fibonacci-anyon representation.
 * Fusion: tau x tau = 1 + tau; golden ratio phi = (1+sqrt 5)/2; on the two-dimensional total-tau
 * fusion space of three tau anyons:
 *   F = [[1/phi, 1/sqrt(phi)], [1/sqrt(phi), -1/phi]],  R = diag(e^{-4 pi i/5}, e^{3 pi i/5})
 *   rho(s1) = R,  rho(s2) = F R F
 * Exact word equivalence uses the reduced Burau representation over Z[t, 1/t], which is faithful
 * for B_3; the Fibonacci matrices give a numerical necessary condition only.
 * Braid-word equality NEVER implies equal physical trajectories: see st_braid_collision. */
#include <complex.h>
#include <stddef.h>
#include "skytrace/core.h"

typedef struct { double complex m[2][2]; } st_mat2;

st_mat2 st_mat2_identity(void);
st_mat2 st_mat2_mul(st_mat2 a, st_mat2 b);
st_mat2 st_mat2_dagger(st_mat2 a);
double st_mat2_max_error(st_mat2 a, st_mat2 b);
double st_mat2_unitarity_error(st_mat2 a); /* max |U U^dagger - I| */

double st_golden_ratio(void);
st_mat2 st_fib_F(void);
st_mat2 st_fib_R(void);
void st_fib_generators(st_mat2 *s1, st_mat2 *s2);

/* Fusion multiplicity N_{ab}^c for labels 0 = vacuum (1), 1 = tau. */
int st_fib_fusion(int a, int b, int c);
/* Dimension of the fusion space of n tau anyons with total charge `total` (0 or 1), by
 * iterating the fusion rules; equals a Fibonacci number. Fails with E_OVERFLOW past uint64. */
st_code st_fib_fusion_space_dim(unsigned n, int total, uint64_t *dim, st_error *err);

#define ST_BRAID_MAX_WORD 4096
/* Letters are +1, +2 (generators) and -1, -2 (inverses = conjugate transposes). */
st_code st_braid_word_matrix(const int *word, size_t n, st_mat2 *out, st_error *err);
/* Free reduction (cancels adjacent x x^-1) in place; returns the new length. */
size_t st_braid_free_reduce(int *word, size_t n);
/* Exact equivalence in B_3 via reduced Burau matrices with int64 Laurent coefficients.
 * E_OVERFLOW if a coefficient would leave int64 (never silently wraps). */
st_code st_braid_equivalent(const int *w1, size_t n1, const int *w2, size_t n2, int *equal, st_error *err);

typedef struct {
  double unitarity_s1, unitarity_s2, braid_relation, inverse_s1, inverse_s2;
  double F_involution, F_unitarity, R_unitarity, golden_identity; /* |phi^2 - phi - 1| */
} st_braid_checks;
void st_braid_run_checks(st_braid_checks *c);

/* ---------- feature modes; every feature set declares its originating sensor inputs ---------- */
typedef enum { ST_FEAT_WORD = 1, ST_FEAT_WORD_TIMED = 2, ST_FEAT_FIB_MATRIX = 3, ST_FEAT_METRIC = 4 } st_feature_mode;
#define ST_FEAT_MAX_INPUTS 16
typedef struct {
  st_feature_mode mode;
  size_t n_letters;
  int letters[ST_BRAID_MAX_WORD];
  double letter_time_s[ST_BRAID_MAX_WORD];   /* mode 2: midpoint time of each crossing */
  double fib_matrix[8];                      /* mode 3: Re then Im of rho(word), row-major */
  double turn_rate_rad_s[ST_BRAID_MAX_WORD]; /* mode 4: |heading rate| per crossing */
  double step_length_m[ST_BRAID_MAX_WORD];   /* mode 4: displacement length per crossing */
  size_t n_inputs;
  char inputs[ST_FEAT_MAX_INPUTS][96];       /* declared sensor inputs (origin ids / digests) */
} st_braid_features;
/* Crossing rule (declared, not inferred): from the fused positions (t, x, y), headings of
 * successive displacements are unwrapped; crossing k uses s1 for even k and s2 for odd k, with
 * the inverse letter when the heading rate is negative. Requires >= 1 declared input. */
st_code st_braid_features_from_track(const double *t, const double *x, const double *y, size_t n,
                                     const char *const *inputs, size_t n_inputs, st_feature_mode mode,
                                     st_braid_features *out, st_error *err);
/* 1 when the two feature sets are equal in their mode although `physical_distance` (a
 * caller-measured difference between physical parameters, e.g. |omega_a - omega_b|) exceeds tol. */
int st_braid_collision(const st_braid_features *a, const st_braid_features *b, double physical_distance, double tol);

#endif
