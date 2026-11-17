#ifndef SKYTRACE_ENTROPY_H
#define SKYTRACE_ENTROPY_H
/* Von Neumann entropy S(rho) = -sum_k lambda_k log2 lambda_k (bits), with eigenvalues from an
 * explicit C11 eigensystem: the n x n Hermitian H = X + iY is embedded as the real symmetric
 * 2n x 2n matrix [[X, -Y], [Y, X]], whose spectrum is that of H with every eigenvalue doubled;
 * the doubled list is diagonalised by cyclic Jacobi (st_sym_eigen) and de-duplicated pairwise. */
#include "skytrace/quantum.h"

st_code st_hermitian_eigenvalues(const st_cmat *h, double *w, st_error *err); /* ascending, n values */
/* Eigenvalues with |lambda| <= tol count as 0; lambda < -tol is E_NOT_POSITIVE. */
st_code st_von_neumann_entropy_bits(const st_cmat *rho, double tol, double *S, st_error *err);

#endif
