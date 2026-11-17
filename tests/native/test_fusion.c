#include "skytrace/fusion.h"
#include "check.h"

static const double S[3][2] = {{-22, -12}, {24, -9}, {-7, 27}};

/* Noise-free synthetic rows from a known truth, reported time = physical + delta_i. */
static size_t make_rows(st_ct_row *rows, const double *truth, const double *delta, int nsens, int with_range, int per) {
  size_t k = 0;
  for (int s = 0; s < nsens; ++s) for (int i = 0; i < per; ++i) {
    double tau = 6.0 * i / (per - 1), x, y; st_ct_position(truth, tau, &x, &y);
    rows[k++] = (st_ct_row){ s, tau + delta[s], S[s][0], S[s][1], atan2(y - S[s][1], x - S[s][0]),
                             with_range ? hypot(x - S[s][0], y - S[s][1]) : NAN };
  }
  return k;
}

int main(void) {
  st_error e; st_ct_options op; st_ct_default_options(&op); st_ct_result res;
  const double truth[5] = {1.2, -0.7, 3.8, 0.4, 0.09}, delta[3] = {0, 0.11, -0.07};
  st_ct_row rows[240];
  /* ODE consistency: finite-difference velocity of the closed form equals (v cos psi, v sin psi). */
  for (double t = 0; t < 6; t += 0.7) {
    double x1, y1, x2, y2, vx, vy, h = 1e-6;
    st_ct_position(truth, t - h, &x1, &y1); st_ct_position(truth, t + h, &x2, &y2); st_ct_velocity(truth, t, &vx, &vy);
    CHECK_NEAR((x2 - x1) / (2 * h), vx, 1e-7); CHECK_NEAR((y2 - y1) / (2 * h), vy, 1e-7);
  }
  double zero_w[5] = {1, 2, 3, 0.5, 0}, x, y; st_ct_position(zero_w, 2, &x, &y);   /* omega = 0 limit */
  CHECK_NEAR(x, 1 + 6 * cos(0.5), 1e-12); CHECK_NEAR(y, 2 + 6 * sin(0.5), 1e-12);

  /* Analytic Jacobian versus central differences (joint model with offsets). */
  size_t n = make_rows(rows, truth, delta, 3, 1, 20);
  st_ct_problem pb = { rows, n, 3, 0.003, 0.10, 1, 1 };
  size_t np = st_ct_nparams(&pb), nr = st_ct_nresiduals(&pb), k;
  CHECK(np == 7 && nr == 120);
  double p[7] = {1.0, -0.5, 3.5, 0.3, 0.07, 0.05, -0.02}, *r = malloc(nr * sizeof *r), *J = malloc(nr * np * sizeof *J), *rp = malloc(nr * sizeof *rp), *rm = malloc(nr * sizeof *rm);
  CHECK(st_ct_residuals(&pb, p, r, J, &k, &e) == ST_E_OK && k == nr);
  double worst = 0;
  for (size_t j = 0; j < np; ++j) {
    double q[7]; memcpy(q, p, sizeof q); double h = 1e-6 * (1 + fabs(p[j]));
    q[j] = p[j] + h; st_ct_residuals(&pb, q, rp, NULL, &k, &e);
    q[j] = p[j] - h; st_ct_residuals(&pb, q, rm, NULL, &k, &e);
    for (size_t i = 0; i < nr; ++i) { double fd = (rp[i] - rm[i]) / (2 * h), d = fabs(fd - J[i*np+j]) / (1 + fabs(fd)); if (d > worst) worst = d; }
  }
  printf("jacobian max relative error vs central differences: %.3e\n", worst);
  CHECK(worst < 1e-5);
  free(r); free(J); free(rp); free(rm);

  /* Exact recovery on noise-free joint data, including clock offsets (reference clock sensor 0). */
  double init[7]; CHECK(st_ct_initial_guess(&pb, init, &e) == ST_E_OK);
  CHECK(st_ct_fit(&pb, init, &op, &res, &e) == ST_E_OK);
  CHECK(res.converged && res.all_identifiable);
  for (int i = 0; i < 5; ++i) CHECK_NEAR(res.x[i], truth[i], 1e-7);
  CHECK_NEAR(res.x[5], delta[1], 1e-7); CHECK_NEAR(res.x[6], delta[2], 1e-7);
  CHECK(res.residual_norm < 1e-6);
  CHECK(res.covariance[0] > 0 && res.covariance[4*7+4] > 0);  /* positive variances */

  /* Bearings-only, three static sensors: no range residual is ever fabricated, geometry still identifies the track. */
  n = make_rows(rows, truth, delta, 3, 0, 20);
  st_ct_problem bo = { rows, n, 3, 0.003, 0.10, 1, 1 };
  CHECK(st_ct_nresiduals(&bo) == 60);  /* bearings only, use_range has nothing to use */
  CHECK(st_ct_initial_guess(&bo, init, &e) == ST_E_RANGE_UNIDENTIFIABLE);
  double start[7] = {1.0, -0.5, 3.5, 0.3, 0.07, 0, 0};
  CHECK(st_ct_fit(&bo, start, &op, &res, &e) == ST_E_OK);
  CHECK(res.converged && res.all_identifiable);
  CHECK_NEAR(res.x[ST_CT_OMEGA], truth[4], 1e-6); CHECK_NEAR(res.x[ST_CT_V], truth[2], 1e-6);

  /* Bearings-only, single static sensor: scaling the track about the sensor preserves every bearing,
   * so position and speed must be reported non-identifiable (no metric speed). */
  n = make_rows(rows, truth, delta, 1, 0, 30);
  st_ct_problem mono = { rows, n, 1, 0.003, 0.10, 0, 0 };
  double s5[5] = {1.1, -0.6, 3.6, 0.38, 0.085};
  CHECK(st_ct_fit(&mono, s5, &op, &res, &e) == ST_E_OK);
  CHECK(!res.all_identifiable);
  CHECK(!res.identifiable[ST_CT_X0] && !res.identifiable[ST_CT_Y0] && !res.identifiable[ST_CT_V]);
  CHECK(res.identifiable[ST_CT_OMEGA]);
  printf("single-sensor bearings-only Fisher eigenvalues: min %.3e max %.3e\n", res.fisher_eig[0], res.fisher_eig[4]);

  /* Gauge: estimating an offset for every sensor would be singular; the API never exposes delta_0. */
  CHECK(st_ct_nparams(&pb) == 5 + 2);
  CHECK_STR(st_ct_param_name(5), "delta_1");
  /* Too few residuals is refused rather than solved. */
  st_ct_problem tiny = { rows, 2, 1, 0.003, 0.10, 0, 0 };
  CHECK(st_ct_fit(&tiny, s5, &op, &res, &e) == ST_E_RANGE_UNIDENTIFIABLE);
  return finish("test_fusion");
}
