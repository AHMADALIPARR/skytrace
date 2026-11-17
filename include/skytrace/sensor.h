#ifndef SKYTRACE_SENSOR_H
#define SKYTRACE_SENSOR_H
/* Sensor observations, angular-rate tracks and the sensor/provenance graph audit that
 * replaces numerics/sensor_graph.py. Output is byte-identical to the Python reference on
 * fixtures/sensors (tests/native/test_sensor.c compares against fixtures/reference). */
#include "skytrace/core.h"
#include "skytrace/graph.h"

/* Python float modulo (result has the sign of the divisor). */
double st_py_fmod(double a, double b);
/* Wraps an angle difference to [-pi, pi) as (d + pi) mod 2pi - pi. */
double st_wrap_angle(double d);

typedef struct { double t_s, az_rad, el_rad; int t_is_int; int64_t t_int; } st_bearing_sample;
typedef struct { double dt_s; int dt_is_int; int64_t dt_int; double az_rad_s, el_rad_s, angular_norm_rad_s; } st_angular_rate;

/* Finite differences of unwrapped angles. Rates are angular (rad/s), never target velocity.
 * Labels: E_SAMPLES_MIN_TWO, E_NONFINITE, E_ELEVATION, E_AZIMUTH, E_TIME_ORDER, E_AZ_UNWRAP_AMBIGUOUS. */
st_code st_bearing_rates(const st_bearing_sample *s, size_t n, st_angular_rate *out, const char **label, st_error *err);

/* One NDJSON request -> one response object ({"status":"ok",...} or {"status":"error","reason":...}). */
st_json *st_sensor_graph_analyze(const st_json *payload);
/* Processes a whole NDJSON stream line by line, writing canonical response lines. */
int st_sensor_graph_stream(const char *text, size_t len, st_buf *out);

#endif
