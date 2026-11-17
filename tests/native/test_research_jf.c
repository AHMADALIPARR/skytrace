// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Ports of research/joint_fusion/test_benchmark.py and research/release_22/test_release.py.
 * Usage: test_research_jf <repo-root> <skytrace-binary> (the binary serves the NDJSON port test). */
#define _POSIX_C_SOURCE 200809L
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include "check.h"

static st_json *fixture(void) {   /* simulate(2, np.random.default_rng(42))['records'] */
  st_np_rng g; st_np_seed(&g, 42); st_jf_row r; st_error e;
  if (st_jf_simulate(2, &g, &r, &e)) return NULL;
  return r.records;
}

int main(int argc, char **argv) {
  const char *root = argc > 1 ? argv[1] : ".", *bin = argc > 2 ? argv[2] : "skytrace";
  st_error e; double f1[64], f2[64]; size_t n1 = 0, n2 = 0;
  st_json *r = fixture(); CHECK(r != NULL);
  if (!r) return finish("research_joint_fusion_release22");
  { st_json *d = st_json_clone(r), *c = NULL; st_json_push(d, st_json_clone(r->items[0]));            /* test_perfect_identity */
    CHECK(st_jf_canon(d, &c, &e) == ST_E_OK && c->count == 3);
    CHECK(st_jf_features(r, ST_JF_FUSED_BRAID, f1, &n1, &e) == ST_E_OK && st_jf_features(d, ST_JF_FUSED_BRAID, f2, &n2, &e) == ST_E_OK); /* test_duplicate_invariance */
    CHECK(n1 == n2); for (size_t i = 0; i < n1; ++i) CHECK(fabs(f1[i] - f2[i]) <= 1e-8 + 1e-5 * fabs(f2[i]));
    st_json_free(c); st_json_free(d); }
  { int distinct = 1; for (size_t i = 0; i < r->count; ++i) for (size_t j = 0; j < i; ++j)              /* test_independent_sensors */
      distinct &= strcmp(st_json_get(r->items[i], "origin_id")->str, st_json_get(r->items[j], "origin_id")->str) != 0;
    CHECK(r->count == 3 && distinct); }
  { double g[ST_JF_GRID], xy[ST_JF_GRID][2]; int rank = 0; CHECK(st_jf_points(r, 1, 1, g, xy, &rank, &e) == ST_E_OK && rank == 2); } /* test_geometry_rank */
  for (int b = 0; b < 4; ++b) { size_t n = 0; CHECK(st_jf_features(r, (st_jf_branch)b, f1, &n, &e) == ST_E_OK); for (size_t i = 0; i < n; ++i) CHECK(isfinite(f1[i])); } /* test_finite_features */
  CHECK(st_jf_features(r, ST_JF_FUSED_BRAID, f1, &n1, &e) == ST_E_OK && st_jf_features(r, ST_JF_FUSED, f2, &n2, &e) == ST_E_OK && n1 > n2); /* test_braid_adds_features */
  { st_json *b = st_json_clone(r), *v = NULL; st_json *s = st_json_get(b->items[0], "samples");         /* test_bad_time_rejected */
    st_json_get(s->items[1], "time_s")->num = st_json_get(s->items[0], "time_s")->num;
    CHECK(st_jf_validate(b, &v, &e) == ST_E_TIME && strstr(e.message, "time")); st_json_free(b); }
  { st_json *b = st_json_clone(r), *v = NULL; st_json_set(st_json_get(b->items[0], "provenance"), "canonical_id", st_json_string("")); /* test_missing_source_rejected */
    CHECK(st_jf_validate(b, &v, &e) != ST_E_OK && strstr(e.message, "provenance")); st_json_free(b); }
  for (int t = 0; t < 7; ++t) { double xa, ya, xb, yb; st_jf_xy_path(t, 1, 2, 3, .6, 0, &xa, &ya); st_jf_xy_path(t, 1, 2, 3, .6, 1e-9, &xb, &yb); /* test_constant_turn_zero_limit */
    CHECK(fabs(xa - xb) <= 1e-7 + 1e-5 * fabs(xb)); CHECK(fabs(ya - yb) <= 1e-7 + 1e-5 * fabs(yb)); }
  { int none = 0; for (size_t i = 0; i < r->count; ++i) { const st_json *s = st_json_get(r->items[i], "samples");       /* test_missing_range_is_preserved */
      for (size_t k = 0; k < s->count; ++k) none |= st_json_get(s->items[k], "range_m")->type == ST_JNULL; }
    CHECK(none); }
  st_json_free(r);

  /* ---- release 2.2 */
  { st_mat2 s1, s2; st_fib_generators(&s1, &s2);                                                       /* test_fibonacci_braid_relation */
    CHECK(st_mat2_max_error(st_mat2_mul(st_mat2_mul(s1, s2), s1), st_mat2_mul(st_mat2_mul(s2, s1), s2)) <= 1e-12); }
  { st_r22_fixture fx; st_r22_physical_fixture(1, &fx); CHECK(sizeof fx.rows / sizeof fx.rows[0] == 60); /* test_matrix_unitary */
    st_np_rng g; st_np_seed(&g, 3); st_jf_row row; double fib[8];
    CHECK(st_jf_simulate(1, &g, &row, &e) == ST_E_OK && st_r22_braid_matrix_features(row.records, fib, &e) == ST_E_OK);
    st_jf_row_free(&row); }
  { st_r22_fixture fx; st_r22_physical_fixture(3, &fx); CHECK(fx.offsets[0] == 0.);                    /* test_reference_clock_identifiability */
    st_r22_fit o; CHECK(st_r22_fit_physics(&fx, 0, &o, &e) == ST_E_OK); CHECK(o.clock_mae_s < .06); CHECK(o.trajectory_rmse_m < .2);
    printf("seed 3 unknown-clock fit: clock MAE %.3g s, trajectory RMSE %.3g m\n", o.clock_mae_s, o.trajectory_rmse_m); }
  { st_r22_fixture fx; st_r22_physical_fixture(5, &fx); double first = NAN;                            /* test_time_measured_not_assumed */
    for (int i = 0; i < 60; ++i) if (fx.rows[i][0] == 1) { first = fx.rows[i][1]; break; }
    CHECK(fabs(first - fx.offsets[1]) <= 1e-8 + 1e-5 * fabs(fx.offsets[1])); }
  { char in[] = "/tmp/skytrace_port_in_XXXXXX"; int fd = mkstemp(in); CHECK(fd >= 0);                   /* test_port_recovery_and_output_count */
    FILE *w = fdopen(fd, "w");
    fputs("{\"operation\": \"physical_fit\", \"seed\": 1, \"job_id\": \"one\"}\n{\"operation\": \"not_supported\", \"seed\": 2}\n{\"operation\": \"physical_fit\", \"seed\": 2, \"job_id\": \"two\"}\n", w);
    fclose(w);
    char cmd[8400]; snprintf(cmd, sizeof cmd, "'%s' port < '%s'", bin, in);
    FILE *p = popen(cmd, "r"); char line[65536]; int k = 0; const char *status[3] = {"ok", "error", "ok"}, *job[3] = {"one", NULL, "two"};
    while (p && fgets(line, sizeof line, p)) {
      st_json *rep = st_json_parse(line, strlen(line), &e); CHECK(rep != NULL);
      if (rep && k < 3) { CHECK_STR(st_json_get(rep, "status")->str, status[k]); if (job[k]) CHECK_STR(st_json_get(rep, "job_id")->str, job[k]); }
      st_json_free(rep); ++k;
    }
    CHECK(p && pclose(p) == 0); CHECK(k == 3); remove(in); }
  { char path[4096]; snprintf(path, sizeof path, "%s/research/joint_fusion/results.synthetic.json", root);  /* test_frozen_reference */
    st_json *saved = st_json_parse_file(path, &e), *now = NULL; CHECK(saved && st_research_joint_fusion(20261117u, &now, &e) == ST_E_OK);
    const st_json *sb = saved ? st_json_get(saved, "branches") : NULL;
    for (size_t i = 0; sb && now && i < sb->count; ++i) {
      double a = st_json_get(sb->items[i], "mae_rad_s")->num, b = st_json_get(st_json_get(st_json_get(now, "branches"), sb->keys[i]), "mae_rad_s")->num;
      CHECK(fabs(a - b) < 1e-10);
    }
    st_json_free(saved); st_json_free(now); }
  return finish("research_joint_fusion_release22");
}
