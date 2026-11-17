#include "skytrace/quantum.h"
#include "skytrace/entropy.h"
#include <math.h>
#include <string.h>

st_code st_cmat_init(st_cmat *m, size_t n, st_error *err) {
  if (n == 0 || n > ST_Q_MAX_DIM) return st_fail(err, ST_E_SHAPE, -1, "matrix dimension %zu out of range", n);
  memset(m, 0, sizeof *m); m->n = n; return ST_E_OK;
}
st_code st_cmat_mul(const st_cmat *a, const st_cmat *b, st_cmat *out, st_error *err) {
  if (a->n != b->n) return st_fail(err, ST_E_SHAPE, -1, "dimension mismatch %zu vs %zu", a->n, b->n);
  st_cmat r; st_cmat_init(&r, a->n, err);
  size_t n = a->n;
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) {
    double complex s = 0; for (size_t k = 0; k < n; ++k) s += a->a[i*n+k] * b->a[k*n+j]; r.a[i*n+j] = s;
  }
  *out = r; return ST_E_OK;
}
void st_cmat_dagger(const st_cmat *a, st_cmat *out) {
  st_cmat r; r.n = a->n; size_t n = a->n;
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) r.a[i*n+j] = conj(a->a[j*n+i]);
  *out = r;
}
st_code st_cmat_kron(const st_cmat *a, const st_cmat *b, st_cmat *out, st_error *err) {
  size_t n = a->n * b->n;
  st_cmat r; st_code rc = st_cmat_init(&r, n, err);
  if (rc) return rc;
  for (size_t i = 0; i < a->n; ++i) for (size_t j = 0; j < a->n; ++j)
    for (size_t k = 0; k < b->n; ++k) for (size_t l = 0; l < b->n; ++l)
      r.a[(i*b->n+k)*n + j*b->n+l] = a->a[i*a->n+j] * b->a[k*b->n+l];
  *out = r; return ST_E_OK;
}
double complex st_cmat_trace(const st_cmat *a) { double complex s = 0; for (size_t i = 0; i < a->n; ++i) s += a->a[i*a->n+i]; return s; }
double st_cmat_max_diff(const st_cmat *a, const st_cmat *b) {
  if (a->n != b->n) return INFINITY;
  double e = 0; for (size_t i = 0; i < a->n * a->n; ++i) { double d = cabs(a->a[i] - b->a[i]); if (d > e) e = d; }
  return e;
}
double st_cmat_hermitian_deviation(const st_cmat *a) {
  double e = 0; size_t n = a->n;
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) { double d = cabs(a->a[i*n+j] - conj(a->a[j*n+i])); if (d > e) e = d; }
  return e;
}

st_code st_state_normalize(double complex *psi, size_t n, st_error *err) {
  double s = 0; for (size_t i = 0; i < n; ++i) { if (!isfinite(creal(psi[i])) || !isfinite(cimag(psi[i]))) return st_fail(err, ST_E_NONFINITE, (long)i, "non-finite amplitude"); s += creal(psi[i] * conj(psi[i])); }
  if (!(s > 0)) return st_fail(err, ST_E_ARGUMENT, -1, "zero vector cannot be normalised");
  double k = 1 / sqrt(s); for (size_t i = 0; i < n; ++i) psi[i] *= k;
  return ST_E_OK;
}
void st_bell_phi_plus(double complex psi[4]) { double h = 1 / sqrt(2.0); psi[0] = h; psi[1] = 0; psi[2] = 0; psi[3] = h; }

st_code st_density_from_state(const double complex *psi, size_t n, st_cmat *rho, st_error *err) {
  st_code rc = st_cmat_init(rho, n, err);
  if (rc) return rc;
  double s = 0; for (size_t i = 0; i < n; ++i) s += creal(psi[i] * conj(psi[i]));
  if (fabs(s - 1) > ST_Q_TOL) return st_fail(err, ST_E_TOLERANCE, -1, "state norm^2 %.17g is not 1 within %g", s, ST_Q_TOL);
  for (size_t i = 0; i < n; ++i) for (size_t j = 0; j < n; ++j) rho->a[i*n+j] = psi[i] * conj(psi[j]);
  return ST_E_OK;
}

st_code st_density_validate(const st_cmat *rho, double tol, st_error *err) {
  double h = st_cmat_hermitian_deviation(rho);
  if (h > tol) return st_fail(err, ST_E_NOT_HERMITIAN, -1, "hermitian deviation %.3g exceeds %.3g", h, tol);
  double complex tr = st_cmat_trace(rho);
  if (cabs(tr - 1) > tol) return st_fail(err, ST_E_TOLERANCE, -1, "trace %.17g is not 1", creal(tr));
  double w[ST_Q_MAX_DIM]; st_code rc = st_hermitian_eigenvalues(rho, w, err);
  if (rc) return rc;
  if (w[0] < -tol) return st_fail(err, ST_E_NOT_POSITIVE, 0, "negative eigenvalue %.3g", w[0]);
  return ST_E_OK;
}

st_code st_partial_trace(const st_cmat *rho, size_t dA, size_t dB, int keep, st_cmat *out, st_error *err) {
  if (dA * dB != rho->n || dA == 0 || dB == 0) return st_fail(err, ST_E_SHAPE, -1, "subsystem dims %zu x %zu do not match %zu", dA, dB, rho->n);
  size_t n = rho->n, d = keep == 0 ? dA : dB;
  st_code rc = st_cmat_init(out, d, err);
  if (rc) return rc;
  for (size_t i = 0; i < d; ++i) for (size_t j = 0; j < d; ++j) {
    double complex s = 0;
    if (keep == 0) for (size_t k = 0; k < dB; ++k) s += rho->a[(i*dB+k)*n + j*dB+k];
    else for (size_t k = 0; k < dA; ++k) s += rho->a[(k*dB+i)*n + k*dB+j];
    out->a[i*d+j] = s;
  }
  return ST_E_OK;
}

static int unitary(const st_cmat *u) {
  st_cmat d, p, id; st_error e; st_cmat_dagger(u, &d); st_cmat_mul(u, &d, &p, &e);
  st_cmat_init(&id, u->n, &e); for (size_t i = 0; i < u->n; ++i) id.a[i*u->n+i] = 1;
  return st_cmat_max_diff(&p, &id) <= ST_Q_TOL;
}

st_code st_apply_local_unitaries(const st_cmat *rho, const st_cmat *UA, const st_cmat *UB, st_cmat *out, st_error *err) {
  if (!unitary(UA) || !unitary(UB)) return st_fail(err, ST_E_TOLERANCE, -1, "local operator is not unitary within %g", ST_Q_TOL);
  st_cmat U, Ud, t; st_code rc = st_cmat_kron(UA, UB, &U, err);
  if (rc) return rc;
  if (U.n != rho->n) return st_fail(err, ST_E_SHAPE, -1, "local unitary dims do not match state");
  st_cmat_dagger(&U, &Ud);
  if ((rc = st_cmat_mul(&U, rho, &t, err))) return rc;
  return st_cmat_mul(&t, &Ud, out, err);
}

void st_observable_xz(double theta, st_cmat *A) {
  st_error e; st_cmat_init(A, 2, &e);
  A->a[0] = cos(theta); A->a[1] = sin(theta); A->a[2] = sin(theta); A->a[3] = -cos(theta);
}

static void projector(double theta, int outcome, st_cmat *P) {
  st_cmat A; st_observable_xz(theta, &A);
  double s = outcome == 0 ? 1.0 : -1.0;
  P->n = 2;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) P->a[i*2+j] = ((i == j) + s * A.a[i*2+j]) / 2;
}

st_code st_joint_probabilities(const st_cmat *rho, double ta, double tb, double p[2][2], st_error *err) {
  if (rho->n != 4) return st_fail(err, ST_E_SHAPE, -1, "two-qubit state required");
  for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) {
    st_cmat Pa, Pb, P, M; projector(ta, a, &Pa); projector(tb, b, &Pb);
    st_code rc = st_cmat_kron(&Pa, &Pb, &P, err); if (rc) return rc;
    if ((rc = st_cmat_mul(rho, &P, &M, err))) return rc;
    double complex tr = st_cmat_trace(&M);
    if (fabs(cimag(tr)) > ST_Q_TOL) return st_fail(err, ST_E_NOT_HERMITIAN, -1, "complex probability");
    p[a][b] = creal(tr);
  }
  return ST_E_OK;
}
st_code st_correlation(const st_cmat *rho, double ta, double tb, double *E, st_error *err) {
  double p[2][2]; st_code rc = st_joint_probabilities(rho, ta, tb, p, err);
  if (!rc) *E = p[0][0] - p[0][1] - p[1][0] + p[1][1];
  return rc;
}
st_code st_chsh(const st_cmat *rho, double a, double a2, double b, double b2, double *S, st_error *err) {
  double e1, e2, e3, e4; st_code rc;
  if ((rc = st_correlation(rho, a, b, &e1, err)) || (rc = st_correlation(rho, a, b2, &e2, err)) ||
      (rc = st_correlation(rho, a2, b, &e3, err)) || (rc = st_correlation(rho, a2, b2, &e4, err))) return rc;
  *S = e1 - e2 + e3 + e4; return ST_E_OK;
}
st_code st_no_signaling_deviation(const st_cmat *rho, const double *al, size_t na, const double *bo, size_t nb, double *dev, st_error *err) {
  double worst = 0;
  for (size_t i = 0; i < na; ++i) for (size_t j = 0; j < nb; ++j) for (size_t k = 0; k < nb; ++k) {
    double p[2][2], q[2][2]; st_code rc;
    if ((rc = st_joint_probabilities(rho, al[i], bo[j], p, err)) || (rc = st_joint_probabilities(rho, al[i], bo[k], q, err))) return rc;
    for (int a = 0; a < 2; ++a) { double d = fabs((p[a][0] + p[a][1]) - (q[a][0] + q[a][1])); if (d > worst) worst = d; }
  }
  for (size_t j = 0; j < nb; ++j) for (size_t i = 0; i < na; ++i) for (size_t k = 0; k < na; ++k) {
    double p[2][2], q[2][2]; st_code rc;
    if ((rc = st_joint_probabilities(rho, al[i], bo[j], p, err)) || (rc = st_joint_probabilities(rho, al[k], bo[j], q, err))) return rc;
    for (int b = 0; b < 2; ++b) { double d = fabs((p[0][b] + p[1][b]) - (q[0][b] + q[1][b])); if (d > worst) worst = d; }
  }
  *dev = worst; return ST_E_OK;
}
st_code st_branch_normalization(const st_cmat *rho, double ta, double tb, double *se, double *mp, st_error *err) {
  double p[2][2]; st_code rc = st_joint_probabilities(rho, ta, tb, p, err);
  if (rc) return rc;
  double s = 0, m = INFINITY;
  for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) { s += p[a][b]; if (p[a][b] < m) m = p[a][b]; }
  *se = fabs(s - 1); *mp = m; return ST_E_OK;
}
