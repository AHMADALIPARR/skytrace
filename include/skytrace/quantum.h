// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_QUANTUM_H
#define SKYTRACE_QUANTUM_H
/* Finite-dimensional quantum states for the Alice-Bob model (mathematical model only).
 * Bell state |Phi+> = (|00> + |11>)/sqrt 2, basis order |ab> with index 2a + b.
 * Measurement along angle theta in the X-Z plane: A(theta) = cos(theta) Z + sin(theta) X,
 * outcome +1/-1 with projectors (I +/- A)/2.
 * Branch weights are Born-rule probabilities of the model. They are mathematical branching,
 * not physical evidence for (or against) the many-worlds interpretation. */
#include <complex.h>
#include <stddef.h>
#include "skytrace/core.h"

#define ST_Q_MAX_DIM 16
typedef struct { size_t n; double complex a[ST_Q_MAX_DIM * ST_Q_MAX_DIM]; } st_cmat; /* square, row-major */

#define ST_Q_TOL 1e-12   /* documented tolerance for every reference check */

st_code st_cmat_init(st_cmat *m, size_t n, st_error *err);       /* zero matrix */
st_code st_cmat_mul(const st_cmat *a, const st_cmat *b, st_cmat *out, st_error *err);
void st_cmat_dagger(const st_cmat *a, st_cmat *out);
st_code st_cmat_kron(const st_cmat *a, const st_cmat *b, st_cmat *out, st_error *err);
double complex st_cmat_trace(const st_cmat *a);
double st_cmat_max_diff(const st_cmat *a, const st_cmat *b);
double st_cmat_hermitian_deviation(const st_cmat *a);

st_code st_state_normalize(double complex *psi, size_t n, st_error *err);
void st_bell_phi_plus(double complex psi[4]);
/* rho = |psi><psi|; psi must have unit norm within ST_Q_TOL. */
st_code st_density_from_state(const double complex *psi, size_t n, st_cmat *rho, st_error *err);
/* Hermitian, unit trace, positive semidefinite (eigenvalues >= -tol). */
st_code st_density_validate(const st_cmat *rho, double tol, st_error *err);
/* keep = 0 returns rho_A = Tr_B rho, keep = 1 returns rho_B = Tr_A rho; dims dA * dB = rho->n. */
st_code st_partial_trace(const st_cmat *rho, size_t dA, size_t dB, int keep, st_cmat *out, st_error *err);
/* (UA (x) UB) rho (UA (x) UB)^dagger; UA, UB must be unitary within ST_Q_TOL. */
st_code st_apply_local_unitaries(const st_cmat *rho, const st_cmat *UA, const st_cmat *UB, st_cmat *out, st_error *err);

void st_observable_xz(double theta, st_cmat *A);
/* p[a][b], a,b in {0: +1, 1: -1}, for two qubits measured at angles ta (Alice), tb (Bob). */
st_code st_joint_probabilities(const st_cmat *rho, double ta, double tb, double p[2][2], st_error *err);
st_code st_correlation(const st_cmat *rho, double ta, double tb, double *E, st_error *err);
/* S = E(a,b) - E(a,b') + E(a',b) + E(a',b'). */
st_code st_chsh(const st_cmat *rho, double a, double a2, double b, double b2, double *S, st_error *err);
/* Max over Alice outcomes/settings of |P(a | ta, tb) - P(a | ta, tb')| and symmetric for Bob. */
st_code st_no_signaling_deviation(const st_cmat *rho, const double *alice, size_t na, const double *bob, size_t nb,
                                  double *deviation, st_error *err);
/* |sum p - 1| and min p for the four branches at (ta, tb). */
st_code st_branch_normalization(const st_cmat *rho, double ta, double tb, double *sum_error, double *min_p, st_error *err);

#endif
