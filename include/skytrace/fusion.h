#ifndef SKYTRACE_FUSION_H
#define SKYTRACE_FUSION_H
/* Calibrated planar constant-turn inference.
 *   dx/dt = v cos(psi(t)),  dy/dt = v sin(psi(t)),  dpsi/dt = omega,  psi(0) = psi0
 * closed form: p(t) = p0 + v t sinc(omega t / 2) [cos(psi0 + omega t/2), sin(psi0 + omega t/2)]
 * Observations from sensor i at reported time t_rep, physical time tau = t_rep - delta_i:
 *   range   r = |p(tau) - s_i(tau)|                      (only when a range was measured)
 *   bearing b = atan2(y - s_iy, x - s_ix)
 * Sensor 0 is the reference clock (delta_0 = 0), removing the common clock-offset gauge.
 * Solver: Levenberg-Marquardt (Marquardt diagonal scaling) on whitened residuals with the
 * analytic Jacobian below; tests/native/test_fusion.c checks it against central differences. */
#include <stddef.h>
#include "skytrace/core.h"

#define ST_CT_MAX_SENSORS 8
#define ST_CT_MAX_PARAMS (5 + ST_CT_MAX_SENSORS - 1)
enum { ST_CT_X0 = 0, ST_CT_Y0, ST_CT_V, ST_CT_PSI, ST_CT_OMEGA, ST_CT_DELTA1 };

typedef struct {
  int sensor;          /* 0 .. nsensors-1; sensor 0 is the reference clock */
  double t_reported_s; /* sensor clock */
  double sensor_x_m, sensor_y_m; /* sensor position at this sample (moving platforms allowed) */
  double bearing_rad;  /* NaN = no bearing in this row */
  double range_m;      /* NaN = no range measured; never synthesised */
} st_ct_row;

typedef struct {
  const st_ct_row *rows; size_t nrows; int nsensors;
  double bearing_sigma_rad, range_sigma_m;
  int estimate_offsets;   /* 1: unknowns delta_1..delta_{S-1}; 0: reported times are physical */
  int use_range;          /* 0: bearings-only even if ranges are present */
} st_ct_problem;

typedef struct { int max_iterations; double ftol, xtol, gtol; } st_ct_options;
void st_ct_default_options(st_ct_options *o); /* 200, 1e-12, 1e-12, 1e-10 */

typedef struct {
  size_t nparams, nresiduals;
  double x[ST_CT_MAX_PARAMS];
  double cost;                     /* 0.5 * sum whitened residual^2 */
  double residual_norm;            /* ||r|| */
  double gradient_inf;             /* ||J^T r||_inf at solution */
  int iterations, nfev, converged;
  const char *stop_reason;         /* "ftol" | "xtol" | "gtol" | "max_iterations" | "damping_overflow" */
  double fisher_eig[ST_CT_MAX_PARAMS];       /* eigenvalues of J^T J ascending */
  double condition_number;
  int identifiable[ST_CT_MAX_PARAMS];        /* 1 identifiable, 0 lies in a near-null direction */
  int all_identifiable;
  double covariance[ST_CT_MAX_PARAMS * ST_CT_MAX_PARAMS]; /* (J^T J)^-1, valid iff all_identifiable */
} st_ct_result;

void st_ct_position(const double *p5, double tau, double *x, double *y);
void st_ct_velocity(const double *p5, double tau, double *vx, double *vy);
/* Whitened residuals r (length nres) and Jacobian J (nres x nparams, row-major; may be NULL). */
st_code st_ct_residuals(const st_ct_problem *pb, const double *params, double *r, double *J, size_t *nres, st_error *err);
size_t st_ct_nparams(const st_ct_problem *pb);
size_t st_ct_nresiduals(const st_ct_problem *pb);
st_code st_ct_fit(const st_ct_problem *pb, const double *init, const st_ct_options *opt, st_ct_result *res, st_error *err);
/* Initial guess used by the 2.2 benchmark: reference-sensor range+bearing positions, first
 * five samples for velocity, linear heading fit for omega, clipped to the benchmark box. */
st_code st_ct_initial_guess(const st_ct_problem *pb, double *init, st_error *err);
const char *st_ct_param_name(size_t i); /* "x0","y0","v","psi","omega","delta_1",... */

#endif
