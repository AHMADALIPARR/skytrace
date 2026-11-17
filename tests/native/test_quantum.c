#include "skytrace/entropy.h"
#include "skytrace/braid.h"
#include "check.h"

static const double PI = 3.14159265358979323846;
static const double TOL = ST_Q_TOL;
static uint32_t rng = 2463534242u;
static double unif(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng + 0.5) / 4294967296.0; }

/* Random SU(2): exp(-i theta n.sigma) = cos(theta) I - i sin(theta) n.sigma */
static void random_unitary(st_cmat *u) {
  st_error e; st_cmat_init(u, 2, &e);
  double th = 2 * PI * unif(), z = 2 * unif() - 1, ph = 2 * PI * unif(), r = sqrt(1 - z * z);
  double nx = r * cos(ph), ny = r * sin(ph), c = cos(th), s = sin(th);
  u->a[0] = c - I * s * z; u->a[1] = -I * s * (nx - I * ny); u->a[2] = -I * s * (nx + I * ny); u->a[3] = c + I * s * z;
}

int main(void) {
  st_error e; double complex psi[4]; st_bell_phi_plus(psi);
  st_cmat rho, rA, rB;
  CHECK(st_density_from_state(psi, 4, &rho, &e) == ST_E_OK);
  CHECK(st_density_validate(&rho, TOL, &e) == ST_E_OK);
  CHECK(st_partial_trace(&rho, 2, 2, 0, &rA, &e) == ST_E_OK && st_partial_trace(&rho, 2, 2, 1, &rB, &e) == ST_E_OK);
  CHECK_NEAR(creal(rA.a[0]), 0.5, TOL); CHECK_NEAR(creal(rA.a[3]), 0.5, TOL); CHECK_NEAR(cabs(rA.a[1]), 0, TOL);
  double S;
  CHECK(st_von_neumann_entropy_bits(&rho, TOL, &S, &e) == ST_E_OK); CHECK_NEAR(S, 0, TOL);    printf("S(rho_AB) = %.17g\n", S);
  CHECK(st_von_neumann_entropy_bits(&rA, TOL, &S, &e) == ST_E_OK); CHECK_NEAR(S, 1, TOL);     printf("S(rho_A)  = %.17g bits\n", S);
  CHECK(st_chsh(&rho, 0, PI / 2, PI / 4, 3 * PI / 4, &S, &e) == ST_E_OK); CHECK_NEAR(S, 2 * sqrt(2.0), TOL); printf("CHSH      = %.17g\n", S);
  /* E(a,b) = cos(a - b) for |Phi+> in the X-Z plane */
  for (double a = -1; a < 1.5; a += 0.37) for (double b = -2; b < 2; b += 0.53) { double E; st_correlation(&rho, a, b, &E, &e); CHECK_NEAR(E, cos(a - b), 1e-13); }
  /* Product state |0>|0> respects the classical CHSH bound for any settings sampled. */
  double complex prod[4] = {1, 0, 0, 0}; st_cmat rp; st_density_from_state(prod, 4, &rp, &e);
  double worst = 0;
  for (int k = 0; k < 2000; ++k) { double v; st_chsh(&rp, 2*PI*unif(), 2*PI*unif(), 2*PI*unif(), 2*PI*unif(), &v, &e); if (fabs(v) > worst) worst = fabs(v); }
  CHECK(worst <= 2 + 1e-12);
  double Sp; st_von_neumann_entropy_bits(&rp, TOL, &Sp, &e); CHECK_NEAR(Sp, 0, TOL);
  /* Maximally mixed two-qubit state: 2 bits. */
  st_cmat mix; st_cmat_init(&mix, 4, &e); for (int i = 0; i < 4; ++i) mix.a[i*5] = 0.25;
  CHECK(st_von_neumann_entropy_bits(&mix, TOL, &S, &e) == ST_E_OK); CHECK_NEAR(S, 2, TOL);
  /* Hermitian eigen-solver on a complex matrix: Pauli Y has eigenvalues -1, +1. */
  st_cmat Y; st_cmat_init(&Y, 2, &e); Y.a[1] = -I; Y.a[2] = I; double w[2];
  CHECK(st_hermitian_eigenvalues(&Y, w, &e) == ST_E_OK); CHECK_NEAR(w[0], -1, 1e-14); CHECK_NEAR(w[1], 1, 1e-14);
  /* Validation failures are machine-readable. */
  st_cmat bad = rho; bad.a[1] = 0.3;  CHECK(st_density_validate(&bad, TOL, &e) == ST_E_NOT_HERMITIAN);
  bad = rho; bad.a[0] = 0.7; CHECK(st_density_validate(&bad, TOL, &e) == ST_E_TOLERANCE);
  st_cmat neg; st_cmat_init(&neg, 2, &e); neg.a[0] = 1.5; neg.a[3] = -0.5; CHECK(st_density_validate(&neg, TOL, &e) == ST_E_NOT_POSITIVE);
  double complex un[4] = {1, 1, 0, 0}; CHECK(st_density_from_state(un, 4, &bad, &e) == ST_E_TOLERANCE);
  CHECK(st_state_normalize(un, 4, &e) == ST_E_OK && st_density_from_state(un, 4, &bad, &e) == ST_E_OK);
  double complex zero[2] = {0, 0}; CHECK(st_state_normalize(zero, 2, &e) == ST_E_ARGUMENT);
  CHECK(st_partial_trace(&rho, 3, 2, 0, &rA, &e) == ST_E_SHAPE);
  /* No-signaling, branch normalization, local-unitary invariance. */
  double al[] = {0, 0.4, PI / 2}, bo[] = {PI / 4, 3 * PI / 4, -1.1}, dev, se, mp;
  CHECK(st_no_signaling_deviation(&rho, al, 3, bo, 3, &dev, &e) == ST_E_OK && dev <= TOL);
  for (int k = 0; k < 200; ++k) {
    st_cmat UA, UB, r2, ra2; random_unitary(&UA); random_unitary(&UB);
    CHECK(st_apply_local_unitaries(&rho, &UA, &UB, &r2, &e) == ST_E_OK);
    CHECK(st_density_validate(&r2, 1e-12, &e) == ST_E_OK);
    st_partial_trace(&r2, 2, 2, 0, &ra2, &e);
    double sa, sab; st_von_neumann_entropy_bits(&ra2, TOL, &sa, &e); st_von_neumann_entropy_bits(&r2, TOL, &sab, &e);
    CHECK(fabs(sa - 1) <= TOL && fabs(sab) <= TOL);
    st_branch_normalization(&r2, 2 * PI * unif(), 2 * PI * unif(), &se, &mp, &e);
    CHECK(se <= TOL && mp >= -TOL);
    st_no_signaling_deviation(&r2, al, 3, bo, 3, &dev, &e); CHECK(dev <= TOL);
  }
  st_cmat nonu; st_cmat_init(&nonu, 2, &e); nonu.a[0] = 2; nonu.a[3] = 1; st_cmat id2; st_cmat_init(&id2, 2, &e); id2.a[0] = id2.a[3] = 1;
  CHECK(st_apply_local_unitaries(&rho, &nonu, &id2, &bad, &e) == ST_E_TOLERANCE);
  /* Fibonacci braid operators on Alice's declared subsystem: entanglement and Bob's state unchanged. */
  int words[3][4] = {{1, 0, 0, 0}, {1, 2, 1, 0}, {2, -1, 2, 2}}; size_t lens[3] = {1, 3, 4};
  for (int k = 0; k < 3; ++k) {
    st_mat2 u; st_braid_word_matrix(words[k], lens[k], &u, &e);
    st_cmat UA, r2, ra2, rb2; st_cmat_init(&UA, 2, &e);
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) UA.a[i*2+j] = u.m[i][j];
    CHECK(st_apply_local_unitaries(&rho, &UA, &id2, &r2, &e) == ST_E_OK);
    st_partial_trace(&r2, 2, 2, 0, &ra2, &e); st_partial_trace(&r2, 2, 2, 1, &rb2, &e);
    double sa; st_von_neumann_entropy_bits(&ra2, TOL, &sa, &e);
    CHECK_NEAR(sa, 1, TOL); CHECK(st_cmat_max_diff(&rb2, &rB) <= TOL);
  }
  /* Braid relation also holds for the operators applied to the subsystem. */
  {
    st_mat2 a, b; int x[] = {1, 2, 1}, y[] = {2, 1, 2}; st_braid_word_matrix(x, 3, &a, &e); st_braid_word_matrix(y, 3, &b, &e);
    st_cmat UA, UB, ra, rb; st_cmat_init(&UA, 2, &e); st_cmat_init(&UB, 2, &e);
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) { UA.a[i*2+j] = a.m[i][j]; UB.a[i*2+j] = b.m[i][j]; }
    st_apply_local_unitaries(&rho, &UA, &id2, &ra, &e); st_apply_local_unitaries(&rho, &UB, &id2, &rb, &e);
    CHECK(st_cmat_max_diff(&ra, &rb) <= TOL);
  }
  return finish("test_quantum");
}
