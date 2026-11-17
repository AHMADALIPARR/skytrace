/* Native report commands. Every number printed is computed in this process run. */
#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include "skytrace/braid.h"
#include "skytrace/entropy.h"
#include "skytrace/evidence.h"
#include "skytrace/fusion.h"
#include "skytrace/graph.h"
#include "skytrace/quantum.h"
#include "skytrace/sensor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

static void emit(st_json *v) {
  st_buf b; st_buf_init(&b); st_json_write(&b, v, ST_JSON_PY_CANONICAL);
  fwrite(b.data, 1, b.len, stdout); fputc('\n', stdout); st_buf_free(&b);
}
static st_json *check(double value, double expected, double tol) {
  st_json *o = st_json_object();
  st_json_set(o, "value", st_json_double(value)); st_json_set(o, "expected", st_json_double(expected));
  st_json_set(o, "abs_error", st_json_double(fabs(value - expected))); st_json_set(o, "tolerance", st_json_double(tol));
  st_json_set(o, "pass", st_json_bool(fabs(value - expected) <= tol));
  return o;
}
static int all_pass(const st_json *checks) {
  for (size_t i = 0; i < checks->count; ++i) { const st_json *p = st_json_get(checks->items[i], "pass"); if (!p || !p->boolean) return 0; }
  return 1;
}

st_code cli_analyze(st_error *err) {
  st_buf in; st_buf_init(&in); char chunk[65536]; size_t n;
  while ((n = fread(chunk, 1, sizeof chunk, stdin)) > 0) {
    st_buf_put(&in, chunk, n);
    if (in.len > ST_JSON_MAX_BYTES) { st_buf_free(&in); return st_fail(err, ST_E_SIZE_LIMIT, -1, "input exceeds limit"); }
  }
  st_buf out; st_buf_init(&out);
  st_sensor_graph_stream(in.data ? in.data : "", in.len, &out);
  fwrite(out.data ? out.data : "", 1, out.len, stdout);
  st_buf_free(&in); st_buf_free(&out); return ST_E_OK;
}

static int code3(st_tri t) { return (int)t; }

st_code cli_records(const char *path, st_error *err) {
  st_json *doc = st_json_parse_file(path, err);
  if (!doc) return err->code;
  st_record_set rs; st_code rc = st_records_load(&rs, doc, err);
  st_json_free(doc);
  if (rc) return rc;
  const char **labels = malloc((rs.n ? rs.n : 1) * sizeof *labels);
  uint32_t *order = malloc((rs.n ? rs.n : 1) * sizeof *order);
  for (size_t i = 0; i < rs.n; ++i) labels[i] = rs.rec[i].canonical_id;
  rc = st_graph_toposort(&rs.contains, labels, order, err);
  st_json *g = st_json_object(), *topo = st_json_array();
  size_t nroots = 0, npaths = 0, maxlen = 0;
  size_t cap = 1u << 20; uint32_t *buf = malloc(cap * sizeof *buf);
  for (size_t i = 0; !rc && i < rs.n; ++i) st_json_push(topo, st_json_string(labels[order[i]]));
  for (size_t i = 0; !rc && i < rs.n; ++i) {
    uint32_t v = order[i];
    if (rs.contains.in_row[v] != rs.contains.in_row[v+1]) continue;
    nroots++;
    size_t np, nw; if ((rc = st_graph_unfold_paths(&rs.contains, labels, v, 10000, buf, cap, &np, &nw, err))) break;
    npaths += np;
    size_t len = 0; for (size_t k = 0; k < nw; ++k) { if (buf[k] == UINT32_MAX) { if (len - 1 > maxlen) maxlen = len - 1; len = 0; } else len++; }
  }
  free(buf);
  size_t ka = 0, sa = 0, ures = 0, ur = 0, uc = 0, ki = 0, si = 0;
  for (size_t i = 0; i < rs.n; ++i) {
    int r = code3(rs.rec[i].range_telemetry), c = code3(rs.rec[i].corroborated), s = code3(rs.rec[i].resolved);
    if (st_case_active(r, c, s)) ka++; else ki++;
    if (st_case_active_strict(r, c, s)) sa++; else si++;
    ures += s == -1; ur += r == -1; uc += c == -1;
  }
  if (!rc) {
    st_json *cl = st_json_object();
    /* Only non-zero counters appear, as with the collections.Counter it replaces. */
    if (ka) st_json_set(cl, "kernel_active", st_json_int((int64_t)ka));
    if (ki) st_json_set(cl, "kernel_inactive", st_json_int((int64_t)ki));
    if (sa) st_json_set(cl, "strict_active", st_json_int((int64_t)sa));
    if (si) st_json_set(cl, "strict_inactive", st_json_int((int64_t)si));
    if (uc) st_json_set(cl, "unknown_corroboration", st_json_int((int64_t)uc));
    if (ur) st_json_set(cl, "unknown_range", st_json_int((int64_t)ur));
    if (ures) st_json_set(cl, "unknown_resolution", st_json_int((int64_t)ures));
    st_json_set(g, "closure", cl);
    st_json_set(g, "edges", st_json_int((int64_t)rs.contains.m));
    st_json_set(g, "max_path_edges", st_json_int((int64_t)maxlen));
    st_json_set(g, "nodes", st_json_int((int64_t)rs.n));
    st_json_set(g, "root_to_leaf_paths", st_json_int((int64_t)npaths));
    st_json_set(g, "roots", st_json_int((int64_t)nroots));
    st_json_set(g, "topological_order", topo); topo = NULL;
    /* Ternary truth table of the two activation policies. */
    st_json *tt = st_json_object(), *mm = st_json_array(); int kc = 0, sc = 0, total = 0;
    for (int r = -1; r <= 1; ++r) for (int c = -1; c <= 1; ++c) for (int s = -1; s <= 1; ++s) {
      total++; kc += st_case_active(r, c, s); sc += st_case_active_strict(r, c, s);
      if (st_case_active(r, c, s) != st_case_active_strict(r, c, s)) {
        st_json *m = st_json_object();
        st_json_set(m, "corroborated", st_json_int(c)); st_json_set(m, "range", st_json_int(r)); st_json_set(m, "resolution", st_json_int(s));
        st_json_push(mm, m);
      }
    }
    st_json_set(tt, "kernel_active_combinations", st_json_int(kc));
    st_json_set(tt, "mismatches", mm);
    st_json_set(tt, "policy_mismatch_combinations", st_json_int((int64_t)st_json_get(tt, "mismatches")->count));
    st_json_set(tt, "strict_active_combinations", st_json_int(sc));
    st_json_set(tt, "ternary_combinations", st_json_int(total));
    st_json *out = st_json_object();
    st_json_set(out, "graph", g); g = NULL;
    st_json_set(out, "ternary_closure", tt);
    st_json_set(out, "observational_data", st_json_bool(0));
    emit(out); st_json_free(out);
  }
  st_json_free(g); st_json_free(topo); free(labels); free(order); st_records_free(&rs);
  return rc;
}

st_code cli_braid_check(st_error *err) {
  st_braid_checks c; st_braid_run_checks(&c);
  const double tol = 1e-12;
  st_json *checks = st_json_array();
  const struct { const char *name; double v; } rows[] = {
    {"unitarity_sigma1", c.unitarity_s1}, {"unitarity_sigma2", c.unitarity_s2}, {"braid_relation_s1s2s1_vs_s2s1s2", c.braid_relation},
    {"inverse_sigma1", c.inverse_s1}, {"inverse_sigma2", c.inverse_s2}, {"F_involution", c.F_involution},
    {"F_unitarity", c.F_unitarity}, {"R_unitarity", c.R_unitarity}, {"golden_ratio_phi2_minus_phi_minus_1", c.golden_identity}};
  for (size_t i = 0; i < sizeof rows / sizeof *rows; ++i) { st_json *o = check(rows[i].v, 0, tol); st_json_set(o, "name", st_json_string(rows[i].name)); st_json_push(checks, o); }
  /* Exact word problem: the braid relation in the faithful Burau representation. */
  int w1[] = {1, 2, 1}, w2[] = {2, 1, 2}, w3[] = {1, 2}, w4[] = {2, 1}, eq12, eq34;
  st_code rc;
  if ((rc = st_braid_equivalent(w1, 3, w2, 3, &eq12, err)) || (rc = st_braid_equivalent(w3, 2, w4, 2, &eq34, err))) { st_json_free(checks); return rc; }
  uint64_t d3, d4; st_fib_fusion_space_dim(3, 1, &d3, err); st_fib_fusion_space_dim(4, 0, &d4, err);
  st_json *o = st_json_object();
  st_json_set(o, "checks", checks);
  st_json *ex = st_json_object();
  st_json_set(ex, "burau_121_equals_212", st_json_bool(eq12)); st_json_set(ex, "burau_12_equals_21", st_json_bool(eq34));
  st_json_set(ex, "fusion_space_dim_3tau_total_tau", st_json_int((int64_t)d3)); st_json_set(ex, "fusion_space_dim_4tau_total_1", st_json_int((int64_t)d4));
  st_json_set(o, "exact", ex);
  int ok = all_pass(checks) && eq12 && !eq34 && d3 == 2 && d4 == 2;
  st_json_set(o, "pass", st_json_bool(ok));
  st_json_set(o, "interpretation", st_json_string("braid-word equivalence is algebraic; it does not imply equal physical trajectories"));
  emit(o); st_json_free(o);
  return ok ? ST_E_OK : st_fail(err, ST_E_TOLERANCE, -1, "braid checks failed");
}

static size_t parse_word(const char *s, int *w) {
  size_t n = 0; const char *p = s;
  while (*p && n < ST_BRAID_MAX_WORD) {
    char *end; long v = strtol(p, &end, 10);
    if (end == p) return SIZE_MAX;
    w[n++] = (int)v; p = end; if (*p == ',') p++; else if (*p) return SIZE_MAX;
  }
  return *p ? SIZE_MAX : n;
}
st_code cli_braid_equiv(const char *a, const char *b, st_error *err) {
  static int w1[ST_BRAID_MAX_WORD], w2[ST_BRAID_MAX_WORD];
  size_t n1 = parse_word(a, w1), n2 = parse_word(b, w2);
  if (n1 == SIZE_MAX || n2 == SIZE_MAX) return st_fail(err, ST_E_ARGUMENT, -1, "words are comma-separated letters from {1,2,-1,-2}");
  int eq; st_code rc = st_braid_equivalent(w1, n1, w2, n2, &eq, err);
  if (rc) return rc;
  st_mat2 m1, m2; if ((rc = st_braid_word_matrix(w1, n1, &m1, err)) || (rc = st_braid_word_matrix(w2, n2, &m2, err))) return rc;
  st_json *o = st_json_object();
  st_json_set(o, "equivalent_in_B3", st_json_bool(eq));
  st_json_set(o, "method", st_json_string("reduced Burau representation over Z[t,1/t] (faithful for B3)"));
  st_json_set(o, "fibonacci_matrix_max_abs_difference", st_json_double(st_mat2_max_error(m1, m2)));
  emit(o); st_json_free(o); return ST_E_OK;
}

st_code cli_quantum_check(st_error *err) {
  double complex psi[4]; st_bell_phi_plus(psi);
  st_cmat rho, rA, rB; st_code rc;
  if ((rc = st_density_from_state(psi, 4, &rho, err)) || (rc = st_density_validate(&rho, ST_Q_TOL, err)) ||
      (rc = st_partial_trace(&rho, 2, 2, 0, &rA, err)) || (rc = st_partial_trace(&rho, 2, 2, 1, &rB, err))) return rc;
  double SAB, SA, SB, S;
  if ((rc = st_von_neumann_entropy_bits(&rho, ST_Q_TOL, &SAB, err)) || (rc = st_von_neumann_entropy_bits(&rA, ST_Q_TOL, &SA, err)) ||
      (rc = st_von_neumann_entropy_bits(&rB, ST_Q_TOL, &SB, err))) return rc;
  if ((rc = st_chsh(&rho, 0, PI / 2, PI / 4, 3 * PI / 4, &S, err))) return rc;
  double al[] = {0, PI / 2, PI / 3}, bo[] = {PI / 4, 3 * PI / 4, 1.0}, nsd, se, mp;
  if ((rc = st_no_signaling_deviation(&rho, al, 3, bo, 3, &nsd, err)) || (rc = st_branch_normalization(&rho, 0, PI / 4, &se, &mp, err))) return rc;
  /* Fibonacci braid word s1 s2 s1^-1 acting on Alice's declared two-dimensional subsystem. */
  st_mat2 u; int word[] = {1, 2, -1};
  if ((rc = st_braid_word_matrix(word, 3, &u, err))) return rc;
  st_cmat UA, UB, rho2, rA2, rB2; st_cmat_init(&UA, 2, err); st_cmat_init(&UB, 2, err);
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) UA.a[i*2+j] = u.m[i][j];
  UB.a[0] = 1; UB.a[3] = 1;
  double SA2, SAB2;
  if ((rc = st_apply_local_unitaries(&rho, &UA, &UB, &rho2, err)) || (rc = st_partial_trace(&rho2, 2, 2, 0, &rA2, err)) ||
      (rc = st_partial_trace(&rho2, 2, 2, 1, &rB2, err)) || (rc = st_von_neumann_entropy_bits(&rA2, ST_Q_TOL, &SA2, err)) ||
      (rc = st_von_neumann_entropy_bits(&rho2, ST_Q_TOL, &SAB2, err))) return rc;
  double nsd2; if ((rc = st_no_signaling_deviation(&rho2, al, 3, bo, 3, &nsd2, err))) return rc;
  st_json *checks = st_json_array();
  const struct { const char *name; double v, e; } rows[] = {
    {"S_rho_AB_bits", SAB, 0}, {"S_rho_A_bits", SA, 1}, {"S_rho_B_bits", SB, 1}, {"CHSH_Tsirelson", S, 2 * sqrt(2.0)},
    {"hermitian_deviation_rho_AB", st_cmat_hermitian_deviation(&rho), 0}, {"trace_rho_AB", creal(st_cmat_trace(&rho)), 1},
    {"no_signaling_max_deviation", nsd, 0}, {"branch_probability_sum_error", se, 0},
    {"braid_on_A_S_rho_A_bits", SA2, 1}, {"braid_on_A_S_rho_AB_bits", SAB2, 0},
    {"braid_on_A_rho_B_unchanged", st_cmat_max_diff(&rB, &rB2), 0}, {"braid_on_A_no_signaling", nsd2, 0}};
  for (size_t i = 0; i < sizeof rows / sizeof *rows; ++i) { st_json *o = check(rows[i].v, rows[i].e, ST_Q_TOL); st_json_set(o, "name", st_json_string(rows[i].name)); st_json_push(checks, o); }
  st_json *o = st_json_object();
  st_json_set(o, "state", st_json_string("Bell |Phi+> = (|00>+|11>)/sqrt2"));
  st_json_set(o, "chsh_settings_rad", st_json_string("a=0, a'=pi/2, b=pi/4, b'=3pi/4"));
  st_json_set(o, "braid_subsystem", st_json_string("Alice qubit identified with the 2-dim total-tau fusion space of three tau anyons; word s1 s2 s1^-1"));
  st_json_set(o, "min_branch_probability", st_json_double(mp));
  st_json_set(o, "checks", checks);
  int ok = all_pass(checks) && mp >= -ST_Q_TOL;
  st_json_set(o, "pass", st_json_bool(ok));
  st_json_set(o, "interpretation", st_json_string("branch weights are mathematical Born-rule probabilities; they are not physical evidence for the many-worlds interpretation"));
  emit(o); st_json_free(o);
  return ok ? ST_E_OK : st_fail(err, ST_E_TOLERANCE, -1, "quantum checks failed");
}

/* ---------- 2.2 physical clock-inference replication ---------- */
static st_code load_case(const st_json *c, st_ct_row **rows, size_t *n, double truth[5], double off[3], st_error *err) {
  const st_json *r = st_json_get(c, "rows"), *t = st_json_get(c, "truth"), *o = st_json_get(c, "offsets");
  if (!r || r->type != ST_JARR || !t || t->count != 5 || !o || o->count != 3) return st_fail(err, ST_E_SCHEMA, -1, "bad fixture case");
  static const double S[3][2] = {{-22, -12}, {24, -9}, {-7, 27}};
  *rows = calloc(r->count, sizeof **rows); *n = r->count;
  for (size_t i = 0; i < 5; ++i) truth[i] = t->items[i]->num;
  for (size_t i = 0; i < 3; ++i) off[i] = o->items[i]->num;
  for (size_t i = 0; i < r->count; ++i) {
    const st_json *x = r->items[i];
    if (x->type != ST_JARR || x->count != 4) return st_fail(err, ST_E_SCHEMA, (long)i, "row must be [sensor,t,bearing,range]");
    int s = (int)x->items[0]->num;
    if (s < 0 || s > 2) return st_fail(err, ST_E_SCHEMA, (long)i, "sensor index");
    (*rows)[i] = (st_ct_row){ s, x->items[1]->num, S[s][0], S[s][1], x->items[2]->num, x->items[3]->num };
  }
  return ST_E_OK;
}
static double traj_rmse(const double *est, const double *truth) {
  double s = 0;
  for (int k = 0; k < 20; ++k) { double t = 6.0 * k / 19, xe, ye, xt, yt; st_ct_position(est, t, &xe, &ye); st_ct_position(truth, t, &xt, &yt); s += (xe-xt)*(xe-xt) + (ye-yt)*(ye-yt); }
  return sqrt(s / 20);
}

st_code cli_fit(const char *ref, const char *pyres, st_error *err) {
  st_json *fx = st_json_parse_file(ref, err); if (!fx) return err->code;
  st_json *pr = st_json_parse_file(pyres, err); if (!pr) { st_json_free(fx); return err->code; }
  const st_json *cases = st_json_get(fx, "cases"), *pf = st_json_get(pr, "physical_fit");
  if (!cases || !pf || cases->count != pf->count) { st_json_free(fx); st_json_free(pr); return st_fail(err, ST_E_SCHEMA, -1, "fixture/result mismatch"); }
  const double TOL = 1e-6;  /* agreement with the frozen SciPy solution (its own xtol/ftol are 1e-8) */
  st_json *out = st_json_array(); int all_ok = 1; st_code rc = ST_E_OK;
  double agg[2][3] = {{0}}; int failed[2] = {0, 0};
  for (size_t ci = 0; ci < cases->count && !rc; ++ci) {
    st_ct_row *rows = NULL; size_t n = 0; double truth[5], off[3];
    if ((rc = load_case(cases->items[ci], &rows, &n, truth, off, err))) break;
    st_json *row = st_json_object();
    st_json_set(row, "seed", st_json_clone(st_json_get(cases->items[ci], "seed")));
    for (int mode = 0; mode < 2 && !rc; ++mode) {   /* 0 = known clock, 1 = unknown offsets */
      st_ct_row *use = rows;
      if (mode == 0) { use = malloc(n * sizeof *use); memcpy(use, rows, n * sizeof *use); for (size_t i = 0; i < n; ++i) use[i].t_reported_s -= off[use[i].sensor]; }
      st_ct_problem pb = { use, n, 3, 0.003, 0.10, mode == 1, 1 };
      double init[ST_CT_MAX_PARAMS]; st_ct_options op; st_ct_default_options(&op); st_ct_result res;
      if (!(rc = st_ct_initial_guess(&pb, init, err))) rc = st_ct_fit(&pb, init, &op, &res, err);
      if (!rc) {
        double est_off[3] = {0, mode ? res.x[5] : off[1], mode ? res.x[6] : off[2]};
        double om = fabs(res.x[4] - truth[4]), cm = (fabs(est_off[1] - off[1]) + fabs(est_off[2] - off[2])) / 2, tr = traj_rmse(res.x, truth);
        const st_json *py = st_json_get(pf->items[ci], mode ? "unknown" : "known");
        double pom = st_json_get(py, "omega_error")->num, pcm = st_json_get(py, "clock_mae_s")->num, ptr = st_json_get(py, "trajectory_rmse_m")->num;
        int conv = res.converged && res.residual_norm < 250;
        int ok = conv && fabs(om - pom) <= TOL && fabs(cm - pcm) <= TOL && fabs(tr - ptr) <= TOL;
        all_ok &= ok; agg[mode][0] += om; agg[mode][1] += cm; agg[mode][2] += tr; failed[mode] += !conv;
        st_json *m = st_json_object();
        st_json_set(m, "omega_error", st_json_double(om)); st_json_set(m, "clock_mae_s", st_json_double(cm));
        st_json_set(m, "trajectory_rmse_m", st_json_double(tr)); st_json_set(m, "converged", st_json_bool(conv));
        st_json_set(m, "iterations", st_json_int(res.iterations)); st_json_set(m, "nfev", st_json_int(res.nfev));
        st_json_set(m, "stop_reason", st_json_string(res.stop_reason)); st_json_set(m, "residual_norm", st_json_double(res.residual_norm));
        st_json_set(m, "condition_number", st_json_double(res.condition_number)); st_json_set(m, "all_identifiable", st_json_bool(res.all_identifiable));
        st_json_set(m, "max_abs_diff_vs_python", st_json_double(fmax(fabs(om - pom), fmax(fabs(cm - pcm), fabs(tr - ptr)))));
        st_json_set(m, "agrees_with_python_within_tol", st_json_bool(ok));
        st_json_set(row, mode ? "unknown" : "known", m);
      }
      if (mode == 0) free(use);
    }
    free(rows); st_json_push(out, row);
  }
  if (!rc) {
    st_json *o = st_json_object(), *ag = st_json_object();
    for (int mode = 0; mode < 2; ++mode) {
      st_json *a = st_json_object(); double k = (double)cases->count;
      st_json_set(a, "omega_error", st_json_double(agg[mode][0] / k)); st_json_set(a, "clock_mae_s", st_json_double(agg[mode][1] / k));
      st_json_set(a, "trajectory_rmse_m", st_json_double(agg[mode][2] / k)); st_json_set(a, "failed", st_json_int(failed[mode]));
      st_json_set(ag, mode ? "unknown" : "known", a);
    }
    st_json_set(o, "physical_fit", out); out = NULL; st_json_set(o, "physical_aggregate", ag);
    st_json_set(o, "tolerance_vs_python", st_json_double(TOL));
    st_json_set(o, "reference_clock", st_json_string("sensor 0 (delta_0 = 0)"));
    st_json_set(o, "pass", st_json_bool(all_ok));
    emit(o); st_json_free(o);
    if (!all_ok) rc = st_fail(err, ST_E_TOLERANCE, -1, "fit disagrees with frozen Python reference");
  }
  st_json_free(out); st_json_free(fx); st_json_free(pr);
  return rc;
}

/* ---------- interchange buffers for the BQN cross-checks ---------- */
static st_code put(const char *dir, const char *name, st_ibuf *b, st_error *err) {
  char p[4400]; snprintf(p, sizeof p, "%s/%s.stbuf", dir, name);
  st_code rc = st_ibuf_write(b, p, err); st_ibuf_free(b); return rc;
}
static st_code put_mat2(const char *dir, const char *name, st_mat2 m, st_error *err) {
  st_ibuf b; uint64_t sh[3] = {2, 2, 2}; st_code rc = st_ibuf_make(&b, ST_IBUF_F64, 3, sh, err);
  if (rc) return rc;
  double *d = b.data;
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) { d[(i*2+j)*2] = creal(m.m[i][j]); d[(i*2+j)*2+1] = cimag(m.m[i][j]); }
  return put(dir, name, &b, err);
}
static st_code put_cmat(const char *dir, const char *name, const st_cmat *m, st_error *err) {
  st_ibuf b; uint64_t sh[3] = {m->n, m->n, 2}; st_code rc = st_ibuf_make(&b, ST_IBUF_F64, 3, sh, err);
  if (rc) return rc;
  double *d = b.data;
  for (size_t i = 0; i < m->n * m->n; ++i) { d[2*i] = creal(m->a[i]); d[2*i+1] = cimag(m->a[i]); }
  return put(dir, name, &b, err);
}

st_code cli_export(const char *dir, st_error *err) {
  st_code rc = st_mkdir_p(dir, err);
  if (rc) return rc;
  st_mat2 s1, s2; st_fib_generators(&s1, &s2);
  int w[] = {1, 2, 2, -1, 2};
  st_mat2 wm; if ((rc = st_braid_word_matrix(w, 5, &wm, err))) return rc;
  if ((rc = put_mat2(dir, "sigma1", s1, err)) || (rc = put_mat2(dir, "sigma2", s2, err)) || (rc = put_mat2(dir, "word_1_2_2_m1_2", wm, err))) return rc;
  double complex psi[4]; st_bell_phi_plus(psi); st_cmat rho, rA;
  if ((rc = st_density_from_state(psi, 4, &rho, err)) || (rc = st_partial_trace(&rho, 2, 2, 0, &rA, err))) return rc;
  if ((rc = put_cmat(dir, "bell_rho_ab", &rho, err)) || (rc = put_cmat(dir, "bell_rho_a", &rA, err))) return rc;
  /* Undirected skeleton of fixtures/sensors/synthetic-graph.json in its node order. */
  st_ibuf g; uint64_t sh[2] = {4, 4};
  if ((rc = st_ibuf_make(&g, ST_IBUF_F64, 2, sh, err))) return rc;
  const int E[4][2] = {{0, 1}, {1, 2}, {1, 3}, {2, 3}}; double *a = g.data;
  for (int k = 0; k < 4; ++k) { a[E[k][0]*4+E[k][1]] = 1; a[E[k][1]*4+E[k][0]] = 1; }
  if ((rc = put(dir, "synthetic_graph_adjacency", &g, err))) return rc;
  /* Constant-turn trajectory samples (t, x, y) for the BQN kinematics cross-check. */
  st_ibuf tr; uint64_t ts[2] = {25, 3};
  if ((rc = st_ibuf_make(&tr, ST_IBUF_F64, 2, ts, err))) return rc;
  const double p5[5] = {1.5, -2.0, 4.0, 0.3, 0.08}; double *d = tr.data;
  for (int k = 0; k < 25; ++k) { double t = 0.25 * k, x, y; st_ct_position(p5, t, &x, &y); d[3*k] = t; d[3*k+1] = x; d[3*k+2] = y; }
  if ((rc = put(dir, "constant_turn_track", &tr, err))) return rc;
  printf("exported 7 interchange buffers to %s\n", dir);
  return ST_E_OK;
}
