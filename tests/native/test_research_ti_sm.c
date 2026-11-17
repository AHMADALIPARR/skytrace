// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Ports of research/task_information/tests/test_benchmark.py and
 * research/speculative_math/tests/test_core.py and research/numerical/tests/test_analyze.py. */
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include "skytrace/quantum.h"
#include "skytrace/entropy.h"
#include "check.h"

static st_json *one(const st_json *rec) { st_json *a = st_json_array(); st_json_push(a, st_json_clone(rec)); return a; }
static double H(const double *p, size_t n) { double s = 0; for (size_t i = 0; i < n; ++i) if (p[i] > 1e-12) s -= p[i] * log2(p[i]); return s; }

int main(void) {
  st_error e; st_ti_row *rows = NULL;
  /* ---- task_information */
  CHECK(st_ti_generate(2, 20261117u, &rows, &e) == ST_E_OK);
  const st_json *row = rows[0].records->items[0], *row2 = rows[1].records->items[0];
  { st_json *a = one(row), *b = one(row); st_json_push(b, st_json_clone(row)); st_ti_repr ra, rb;          /* test_dup */
    CHECK(st_ti_representation(a, &ra, &e) == ST_E_OK && st_ti_representation(b, &rb, &e) == ST_E_OK);
    CHECK_STR(ra.fingerprint, rb.fingerprint); st_json_free(a); st_json_free(b); }
  { st_json *a = one(row), *ch = st_json_clone(row); st_json_get(ch, "angles")->items[1]->num += .01; st_json_push(a, ch); /* test_mutated_content_same_origin_not_lost */
    st_json *c = NULL; CHECK(st_ti_canonical_origins(a, &c, &e) == ST_E_OK && c->count == 2); st_json_free(c); st_json_free(a); }
  { st_json *ab = one(row), *ba = one(row2); st_json_push(ab, st_json_clone(row2)); st_json_push(ba, st_json_clone(row)); /* test_shuffled_order */
    st_ti_repr r1, r2; CHECK(st_ti_representation(ab, &r1, &e) == ST_E_OK && st_ti_representation(ba, &r2, &e) == ST_E_OK);
    CHECK_STR(r1.fingerprint, r2.fingerprint); st_json_free(ab); st_json_free(ba); }
  CHECK(st_json_get(row, "range_m")->type == ST_JNULL);                                                   /* test_no_speed_from_range_unknown */
  { st_json *a = one(row), *t = st_json_array(); for (int i = 0; i < 9; ++i) st_json_push(t, st_json_int(0));  /* test_time_invalid */
    st_json_set(a->items[0], "times", t); st_ti_repr r; CHECK(st_ti_representation(a, &r, &e) == ST_E_TIME); st_json_free(a); }
  st_ti_free(rows, 2);
  { st_json *b1 = NULL, *b2 = NULL; CHECK(st_research_task_information(&b1, &e) == ST_E_OK && st_research_task_information(&b2, &e) == ST_E_OK);
    CHECK(st_json_equal(b1, b2));                                                                         /* test_reproducible */
    CHECK(st_json_get(b1, "duplication_invariant")->boolean); CHECK(st_json_get(b1, "different_measurement_retained")->boolean); /* test_comparison */
    const st_json *m = st_json_get(b1, "models");
    CHECK(st_json_get(m, "temporal_signed_rate_baseline")->num >= st_json_get(m, "lossy_absolute_magnitude_baseline")->num);
    st_json_free(b1); st_json_free(b2); }
  { int w[4] = {1, 2, -1, 2}; st_mat2 U; CHECK(st_braid_word_matrix(w, 4, &U, &e) == ST_E_OK);          /* test_braid_unitary_entropy */
    st_cmat u, rho, ud, t1, rot; st_cmat_init(&u, 2, &e); st_cmat_init(&rho, 2, &e);
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) u.a[i*2+j] = U.m[i][j];
    rho.a[0] = .7; rho.a[3] = .3;
    st_cmat_dagger(&u, &ud); st_cmat_mul(&u, &rho, &t1, &e); st_cmat_mul(&t1, &ud, &rot, &e);
    double w2[2]; CHECK(st_hermitian_eigenvalues(&rot, w2, &e) == ST_E_OK);
    const double p[2] = {.7, .3};
    CHECK_NEAR(H(w2, 2), H(p, 2), 5e-13); }

  /* ---- speculative_math */
  { int a[3] = {1, 2, 1}, b[3] = {2, 1, 2}; st_mat2 x, y; st_braid_word_matrix(a, 3, &x, &e); st_braid_word_matrix(b, 3, &y, &e);
    CHECK(st_mat2_max_error(x, y) < 1e-12); }                                                             /* test_artin_relation */
  { st_mat2 s1, s2; st_fib_generators(&s1, &s2);                                                          /* test_unitary */
    CHECK(st_mat2_max_error(st_mat2_mul(s1, st_mat2_dagger(s1)), st_mat2_identity()) < 1e-12);
    CHECK(st_mat2_max_error(st_mat2_mul(s2, st_mat2_dagger(s2)), st_mat2_identity()) < 1e-12); }
  { int w[4] = {1, -1, 2, -2}; st_mat2 x; st_braid_word_matrix(w, 4, &x, &e); CHECK(st_mat2_max_error(x, st_mat2_identity()) < 1e-12); } /* test_inverse */
  { int w[1] = {3}; st_mat2 x; CHECK(st_braid_word_matrix(w, 1, &x, &e) == ST_E_ARGUMENT); }             /* test_bad_generator */
  { const char *n[4] = {"a", "b", "c", "d"}; const char *E[4][2] = {{"a", "b"}, {"a", "c"}, {"b", "d"}, {"c", "d"}};  /* test_dag_paths */
    size_t out[32], np = 0;
    CHECK(st_sm_unfold_paths(n, 4, (const char *const (*)[2])E, 4, "a", 10000, out, 32, &np, &e) == ST_E_OK && np == 2);
    const size_t want[8] = {0, 1, 3, (size_t)-1, 0, 2, 3, (size_t)-1};
    CHECK(!memcmp(out, want, sizeof want)); }
  { const char *n[2] = {"a", "b"}; const char *E[2][2] = {{"a", "b"}, {"b", "a"}}; size_t ord[2];       /* test_cycle */
    CHECK(st_sm_validate_dag(n, 2, (const char *const (*)[2])E, 2, ord, &e) == ST_E_CYCLE); }
  { const char *n[3] = {"a", "b", "c"}; const char *E[2][2] = {{"a", "b"}, {"a", "c"}}; size_t out[16], np;  /* test_path_cap */
    CHECK(st_sm_unfold_paths(n, 3, (const char *const (*)[2])E, 2, "a", 1, out, 16, &np, &e) == ST_E_OVERFLOW); }
  { int bits[4] = {1, 0, 1, 1}, code[5], s = -1; CHECK(st_sm_parity_encode(bits, 4, code, &e) == ST_E_OK); /* test_parity */
    CHECK(st_sm_parity_syndrome(code, 5, &s, &e) == ST_E_OK && s == 0);
    code[2] ^= 1; CHECK(st_sm_parity_syndrome(code, 5, &s, &e) == ST_E_OK && s == 1); }
  { int bits[3] = {1, 0, 1}, code[4], s = -1; st_sm_parity_encode(bits, 3, code, &e); code[0] ^= 1; code[1] ^= 1; /* test_parity_even_flips_limit */
    CHECK(st_sm_parity_syndrome(code, 4, &s, &e) == ST_E_OK && s == 0); }
  { st_sm_probe st[16]; size_t n = 0; CHECK(st_sm_replicate_probes(1, 7, 2, 2, 6, st, 16, &n, &e) == ST_E_OK); /* test_probes_resource_bound */
    CHECK(st[n-1].count == 4); CHECK(st[n-1].resource_units == 1); CHECK(n == 7); }
  { const char *l[2] = {"earth", "mars"}; const char *B[1][2] = {{"earth", "mars"}}; st_json *o = NULL;  /* test_bridge_is_not_physical_claim */
    CHECK(st_sm_bridge_edges(l, 2, (const char *const (*)[2])B, 1, &o, &e) == ST_E_OK);
    CHECK_STR(st_json_get(o, "status")->str, "speculative_not_observed"); st_json_free(o); }

  /* ---- numerical closure audit */
  { st_json *t = st_num_truth_table();                                                                  /* test_ternary_domain */
    CHECK(st_json_get(t, "ternary_combinations")->ival == 27); CHECK(st_json_get(t, "policy_mismatch_combinations")->ival == 8); st_json_free(t); }
  CHECK(st_num_kernel_active(-1, -1, -1)); CHECK(!st_num_strict_active(-1, -1, -1));                   /* test_unknown_resolution */
  for (int rr = -1; rr <= 1; ++rr) for (int c = -1; c <= 1; ++c) CHECK(!st_num_kernel_active(rr, c, 1)); /* test_resolved_never_active */
  { const char *txt = "[{\"canonical_id\":\"a\",\"parent_id\":\"b\"},{\"canonical_id\":\"b\",\"parent_id\":\"a\"}]"; /* test_cycle_detected */
    st_json *recs = st_json_parse(txt, strlen(txt), &e), *m = NULL; CHECK(recs && st_num_metrics(recs, &m, &e) == ST_E_CYCLE); st_json_free(recs); st_json_free(m); }
  { st_json *b = NULL; CHECK(st_num_braid_audit(&b, &e) == ST_E_OK); CHECK(st_json_get(b, "braid_relation_max_abs_error")->num < 1e-12); st_json_free(b); } /* test_braid_numerical */
  return finish("research_task_information_speculative_math_numerical");
}
