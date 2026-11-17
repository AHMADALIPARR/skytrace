#include "skytrace/fusion.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void st_ct_default_options(st_ct_options *o) { o->max_iterations = 200; o->ftol = 1e-12; o->xtol = 1e-12; o->gtol = 1e-10; }

static const char *const PNAMES[] = {"x0","y0","v","psi","omega","delta_1","delta_2","delta_3","delta_4","delta_5","delta_6","delta_7"};
const char *st_ct_param_name(size_t i) { return i < sizeof PNAMES / sizeof *PNAMES ? PNAMES[i] : "?"; }

/* sinc(u) = sin(u)/u and its derivative, with series near 0. */
static void sinc_d(double u, double *s, double *ds) {
  if (fabs(u) < 1e-4) { double u2 = u * u; *s = 1 - u2 / 6 + u2 * u2 / 120; *ds = -u / 3 + u * u2 / 30; }
  else { *s = sin(u) / u; *ds = (u * cos(u) - sin(u)) / (u * u); }
}

void st_ct_position(const double *p, double tau, double *x, double *y) {
  double u = p[ST_CT_OMEGA] * tau / 2, s, ds; sinc_d(u, &s, &ds);
  double phi = p[ST_CT_PSI] + u;
  *x = p[ST_CT_X0] + p[ST_CT_V] * tau * s * cos(phi);
  *y = p[ST_CT_Y0] + p[ST_CT_V] * tau * s * sin(phi);
}
void st_ct_velocity(const double *p, double tau, double *vx, double *vy) {
  double h = p[ST_CT_PSI] + p[ST_CT_OMEGA] * tau;
  *vx = p[ST_CT_V] * cos(h); *vy = p[ST_CT_V] * sin(h);
}

size_t st_ct_nparams(const st_ct_problem *pb) { return 5 + (pb->estimate_offsets ? (size_t)(pb->nsensors - 1) : 0); }
size_t st_ct_nresiduals(const st_ct_problem *pb) {
  size_t n = 0;
  for (size_t i = 0; i < pb->nrows; ++i) { n += isfinite(pb->rows[i].bearing_rad) ? 1 : 0; n += pb->use_range && isfinite(pb->rows[i].range_m) ? 1 : 0; }
  return n;
}

st_code st_ct_residuals(const st_ct_problem *pb, const double *p, double *r, double *J, size_t *nres, st_error *err) {
  size_t np = st_ct_nparams(pb), k = 0;
  if (pb->nsensors < 1 || pb->nsensors > ST_CT_MAX_SENSORS) return st_fail(err, ST_E_ARGUMENT, -1, "sensor count out of range");
  if (!(pb->bearing_sigma_rad > 0) || (pb->use_range && !(pb->range_sigma_m > 0))) return st_fail(err, ST_E_CALIBRATION, -1, "measurement sigmas must be positive");
  for (size_t i = 0; i < pb->nrows; ++i) {
    const st_ct_row *row = &pb->rows[i];
    if (row->sensor < 0 || row->sensor >= pb->nsensors) return st_fail(err, ST_E_ARGUMENT, (long)i, "row sensor index out of range");
    double delta = (pb->estimate_offsets && row->sensor > 0) ? p[ST_CT_DELTA1 + row->sensor - 1] : 0.0;
    double tau = row->t_reported_s - delta;
    double u = p[ST_CT_OMEGA] * tau / 2, s, ds; sinc_d(u, &s, &ds);
    double phi = p[ST_CT_PSI] + u, c = cos(phi), sn = sin(phi), v = p[ST_CT_V];
    double x = p[ST_CT_X0] + v * tau * s * c, y = p[ST_CT_Y0] + v * tau * s * sn;
    double dx = x - row->sensor_x_m, dy = y - row->sensor_y_m, r2 = dx * dx + dy * dy, rr = sqrt(r2);
    if (!(rr > 0)) return st_fail(err, ST_E_RANGE_UNIDENTIFIABLE, (long)i, "target coincides with sensor");
    /* d(x,y)/d(params) */
    double gx[ST_CT_MAX_PARAMS] = {0}, gy[ST_CT_MAX_PARAMS] = {0};
    gx[ST_CT_X0] = 1; gy[ST_CT_Y0] = 1;
    gx[ST_CT_V] = tau * s * c; gy[ST_CT_V] = tau * s * sn;
    gx[ST_CT_PSI] = -v * tau * s * sn; gy[ST_CT_PSI] = v * tau * s * c;
    gx[ST_CT_OMEGA] = v * tau * (ds * tau / 2 * c - s * sn * tau / 2);
    gy[ST_CT_OMEGA] = v * tau * (ds * tau / 2 * sn + s * c * tau / 2);
    if (pb->estimate_offsets && row->sensor > 0) {
      double vx, vy; st_ct_velocity(p, tau, &vx, &vy);
      gx[ST_CT_DELTA1 + row->sensor - 1] = -vx; gy[ST_CT_DELTA1 + row->sensor - 1] = -vy;
    }
    if (isfinite(row->bearing_rad)) {
      double b = atan2(dy, dx), e = b - row->bearing_rad;
      r[k] = atan2(sin(e), cos(e)) / pb->bearing_sigma_rad;
      if (J) for (size_t j = 0; j < np; ++j) J[k*np+j] = (-dy / r2 * gx[j] + dx / r2 * gy[j]) / pb->bearing_sigma_rad;
      k++;
    }
    if (pb->use_range && isfinite(row->range_m)) {
      r[k] = (rr - row->range_m) / pb->range_sigma_m;
      if (J) for (size_t j = 0; j < np; ++j) J[k*np+j] = (dx / rr * gx[j] + dy / rr * gy[j]) / pb->range_sigma_m;
      k++;
    }
  }
  *nres = k;
  for (size_t i = 0; i < k; ++i) if (!isfinite(r[i])) return st_fail(err, ST_E_NONFINITE, (long)i, "non-finite residual");
  return ST_E_OK;
}

static double half_sq(const double *r, size_t n) { double s = 0; for (size_t i = 0; i < n; ++i) s += r[i] * r[i]; return 0.5 * s; }

st_code st_ct_fit(const st_ct_problem *pb, const double *init, const st_ct_options *opt, st_ct_result *res, st_error *err) {
  memset(res, 0, sizeof *res);
  size_t np = st_ct_nparams(pb), nr = st_ct_nresiduals(pb);
  if (nr < np) return st_fail(err, ST_E_RANGE_UNIDENTIFIABLE, -1, "%zu residuals cannot determine %zu parameters", nr, np);
  double *r = malloc(nr * sizeof *r), *J = malloc(nr * np * sizeof *J), *rt = malloc(nr * sizeof *rt);
  if (!r || !J || !rt) { free(r); free(J); free(rt); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  double x[ST_CT_MAX_PARAMS], A[ST_CT_MAX_PARAMS * ST_CT_MAX_PARAMS], g[ST_CT_MAX_PARAMS];
  memcpy(x, init, np * sizeof *x);
  size_t k; st_code rc = st_ct_residuals(pb, x, r, J, &k, err);
  res->nfev = 1;
  double cost = half_sq(r, nr), lambda = 1e-3;
  res->stop_reason = "max_iterations";
  for (int it = 0; !rc && it < opt->max_iterations; ++it) {
    res->iterations = it + 1;
    for (size_t a = 0; a < np; ++a) {
      double s = 0; for (size_t i = 0; i < nr; ++i) s += J[i*np+a] * r[i]; g[a] = s;
      for (size_t b = 0; b < np; ++b) { double t = 0; for (size_t i = 0; i < nr; ++i) t += J[i*np+a] * J[i*np+b]; A[a*np+b] = t; }
    }
    double ginf = 0; for (size_t a = 0; a < np; ++a) if (fabs(g[a]) > ginf) ginf = fabs(g[a]);
    if (ginf <= opt->gtol) { res->converged = 1; res->stop_reason = "gtol"; break; }
    int accepted = 0;
    while (!accepted) {
      double M[ST_CT_MAX_PARAMS * ST_CT_MAX_PARAMS], neg[ST_CT_MAX_PARAMS], dx[ST_CT_MAX_PARAMS], xt[ST_CT_MAX_PARAMS];
      memcpy(M, A, np * np * sizeof *M);
      for (size_t a = 0; a < np; ++a) { M[a*np+a] += lambda * (A[a*np+a] > 0 ? A[a*np+a] : 1e-12); neg[a] = -g[a]; }
      st_error se;
      if (st_cholesky_solve(M, np, neg, dx, &se)) { lambda *= 10; if (lambda > 1e16) break; continue; }
      double xn = 0, dn = 0;
      for (size_t a = 0; a < np; ++a) { xt[a] = x[a] + dx[a]; xn += x[a] * x[a]; dn += dx[a] * dx[a]; }
      size_t kk; st_error e2;
      res->nfev++;
      if (st_ct_residuals(pb, xt, rt, NULL, &kk, &e2)) { lambda *= 10; if (lambda > 1e16) break; continue; }
      double ct = half_sq(rt, nr);
      if (ct < cost) {
        accepted = 1;
        double rel = (cost - ct) / (cost > 0 ? cost : 1);
        memcpy(x, xt, np * sizeof *x); cost = ct;
        lambda = lambda / 10 < 1e-12 ? 1e-12 : lambda / 10;
        rc = st_ct_residuals(pb, x, r, J, &k, err);
        if (rel <= opt->ftol) { res->converged = 1; res->stop_reason = "ftol"; }
        else if (sqrt(dn) <= opt->xtol * (opt->xtol + sqrt(xn))) { res->converged = 1; res->stop_reason = "xtol"; }
      } else {
        lambda *= 10;
        if (sqrt(dn) <= opt->xtol * (opt->xtol + sqrt(xn))) { res->converged = 1; res->stop_reason = "xtol"; break; }
        if (lambda > 1e16) break;
      }
    }
    if (!accepted && !res->converged) { res->stop_reason = "damping_overflow"; break; }
    if (res->converged) break;
  }
  if (!rc) {
    res->nparams = np; res->nresiduals = nr; memcpy(res->x, x, np * sizeof *x);
    res->cost = cost; res->residual_norm = sqrt(2 * cost);
    for (size_t a = 0; a < np; ++a) {
      double s = 0; for (size_t i = 0; i < nr; ++i) s += J[i*np+a] * r[i]; g[a] = s;
      for (size_t b = 0; b < np; ++b) { double t = 0; for (size_t i = 0; i < nr; ++i) t += J[i*np+a] * J[i*np+b]; A[a*np+b] = t; }
    }
    res->gradient_inf = 0; for (size_t a = 0; a < np; ++a) if (fabs(g[a]) > res->gradient_inf) res->gradient_inf = fabs(g[a]);
    /* Identifiability: eigen-structure of the Fisher information J^T J (whitened units). */
    double E[ST_CT_MAX_PARAMS * ST_CT_MAX_PARAMS], V[ST_CT_MAX_PARAMS * ST_CT_MAX_PARAMS];
    memcpy(E, A, np * np * sizeof *E);
    rc = st_sym_eigen(E, np, res->fisher_eig, V, err);
    if (!rc) {
      double lmax = res->fisher_eig[np-1], lmin = res->fisher_eig[0];
      res->condition_number = lmin > 0 ? lmax / lmin : INFINITY;
      res->all_identifiable = 1;
      for (size_t a = 0; a < np; ++a) res->identifiable[a] = 1;
      for (size_t e = 0; e < np; ++e) if (!(res->fisher_eig[e] > 1e-10 * lmax)) {
        res->all_identifiable = 0;
        for (size_t a = 0; a < np; ++a) if (fabs(V[a*np+e]) > 0.1) res->identifiable[a] = 0;
      }
      if (res->all_identifiable && st_spd_inverse(A, np, res->covariance, err)) { res->all_identifiable = 0; st_error_clear(err); }
    }
  }
  free(r); free(J); free(rt);
  return rc;
}

st_code st_ct_initial_guess(const st_ct_problem *pb, double *init, st_error *err) {
  /* Reference-sensor rows in input order. */
  size_t n = 0; double t[4096], px[4096], py[4096];
  for (size_t i = 0; i < pb->nrows && n < 4096; ++i) {
    const st_ct_row *w = &pb->rows[i];
    if (w->sensor != 0 || !isfinite(w->range_m) || !isfinite(w->bearing_rad)) continue;
    t[n] = w->t_reported_s; px[n] = w->sensor_x_m + w->range_m * cos(w->bearing_rad); py[n] = w->sensor_y_m + w->range_m * sin(w->bearing_rad); n++;
  }
  if (n < 5) return st_fail(err, ST_E_RANGE_UNIDENTIFIABLE, -1, "initial guess needs >= 5 reference-sensor range+bearing rows");
  double vx = (px[4] - px[0]) / (t[4] - t[0]), vy = (py[4] - py[0]) / (t[4] - t[0]);
  double speed = hypot(vx, vy), psi = atan2(vy, vx);
  /* np.unwrap of segment headings, then least-squares slope against segment mid-times. */
  double h[4096], tm[4096], raw_prev = 0;
  const double PI = 3.14159265358979323846;
  for (size_t i = 0; i + 1 < n; ++i) {
    double raw = atan2(py[i+1] - py[i], px[i+1] - px[i]), a = raw;
    if (i) { double d = raw - raw_prev, dd = fmod(d + PI, 2 * PI); if (dd < 0) dd += 2 * PI; dd -= PI;
             if (dd == -PI && d > 0) { dd = PI; }
             if (fabs(d) < PI) { dd = d; }
             a = h[i-1] + dd; }
    h[i] = a; raw_prev = raw; tm[i] = (t[i+1] + t[i]) / 2;
  }
  size_t m = n - 1; double mt = 0, mh = 0, stt = 0, sth = 0;
  for (size_t i = 0; i < m; ++i) { mt += tm[i]; mh += h[i]; }
  mt /= (double)m; mh /= (double)m;
  for (size_t i = 0; i < m; ++i) { stt += (tm[i] - mt) * (tm[i] - mt); sth += (tm[i] - mt) * (h[i] - mh); }
  double w0 = stt > 0 ? sth / stt : 0;
  init[0] = px[0]; init[1] = py[0];
  init[2] = speed < 1 ? 1 : speed > 6 ? 6 : speed; init[3] = psi;
  init[4] = w0 < -0.2 ? -0.2 : w0 > 0.2 ? 0.2 : w0;
  for (size_t i = 5; i < st_ct_nparams(pb); ++i) init[i] = 0;
  return ST_E_OK;
}
