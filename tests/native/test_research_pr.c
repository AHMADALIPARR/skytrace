// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Ports of research/parameter_recovery/tests/test_benchmark.py, research/graph_budget/test_benchmark.py
 * and research/source_fusion/test_fusion.py. */
#include "skytrace/research.h"
#include "check.h"

static int feats(const st_json *r, st_pr_branch b, double *f, size_t *n) { st_error e; return st_pr_features(r, b, f, n, &e) == ST_E_OK; }
static int same(const st_json *a, const st_json *b, st_pr_branch br) {
  double f1[64], f2[64]; size_t n1 = 0, n2 = 0;
  if (!feats(a, br, f1, &n1) || !feats(b, br, f2, &n2) || n1 != n2) return 0;
  for (size_t i = 0; i < n1; ++i) if (f1[i] != f2[i]) return 0;
  return 1;
}
static st_json *with_dup(const st_json *r) { st_json *d = st_json_clone(r); st_json_push(d, st_json_clone(r->items[0])); return d; }
static double *pos(st_json *records, size_t i, size_t k) { return &st_json_get(records->items[0], "positions_m")->items[i]->items[k]->num; }
static double *tim(st_json *records, size_t i) { return &st_json_get(records->items[0], "times_s")->items[i]->num; }

int main(int argc, char **argv) {
  const char *root = argc > 1 ? argv[1] : ".";
  st_error e; st_pr_data d; double f[64]; size_t nf;
  CHECK(st_pr_generate(&d, &e) == ST_E_OK);
  /* ---- parameter_recovery */
  CHECK(d.ntrain == 220 && d.nval == 70 && d.ntest == 70);                               /* test_split_sizes */
  { int seen[360] = {0}, uniq = 0;                                                        /* test_disjoint_trajectory_ids */
    for (int w = 0; w < 3; ++w) for (size_t i = 0; i < st_pr_split_n(&d, w); ++i) { int id = st_pr_split(&d, w, i)->id; if (!seen[id]++) uniq++; }
    CHECK(uniq == 360); }
  const st_json *tr0 = st_pr_split(&d, 0, 0)->records;
  { st_json *dup = with_dup(tr0);                                                         /* test_invariant_exact_duplicate */
    for (int b = 0; b <= ST_PR_BRAID_METRIC; ++b) CHECK(same(tr0, dup, (st_pr_branch)b));
    st_json_free(dup); }
  { st_json *both = st_json_clone(tr0), *cp = st_json_clone(tr0->items[0]);              /* test_changed_measurement_not_merged */
    st_json_get(cp, "positions_m")->items[2]->items[0]->num += .1; st_json_push(both, cp);
    st_json *canon = NULL; CHECK(st_pr_canonicalize(both, &canon, &e) == ST_E_OK && canon && canon->count == 2);
    st_json_free(canon); st_json_free(both); }
  { st_pr_collision c; CHECK(st_pr_collision_example(&c, &e) == ST_E_OK); CHECK(c.same_braid_word); CHECK(!c.same_temporal_features); } /* test_word_collision */
  { st_np_rng g; st_np_seed(&g, 233); st_pr_row row; double om;                         /* test_zero_noise_reference */
    CHECK(st_pr_simulate(9, &g, 18, 0, 0, 0, &row, &e) == ST_E_OK);
    CHECK(st_pr_direct_fit(row.records, &om, &e) == ST_E_OK); CHECK(fabs(om - row.theta_rad_s) < 1e-5);
    printf("zero-noise direct fit |omega error| = %.3g\n", fabs(om - row.theta_rad_s));
    st_pr_row_free(&row); }
  { st_json *r = st_json_clone(tr0); *tim(r, 4) = *tim(r, 3);                            /* test_invalid_timestamps */
    CHECK(st_pr_features(r, ST_PR_TEMPORAL, f, &nf, &e) == ST_E_TIME); st_json_free(r); }
  { size_t a = 0, b = 0, c = 0; feats(tr0, ST_PR_BRAID_METRIC, f, &a); feats(tr0, ST_PR_BRAID_WORD, f, &b); feats(tr0, ST_PR_TEMPORAL, f, &c);
    CHECK(a == b + c && a == 19); }                                                       /* test_branch_feature_lengths */
  for (size_t i = 0; i < 5; ++i) { CHECK(feats(st_pr_split(&d, 2, i)->records, ST_PR_TEMPORAL, f, &nf)); for (size_t k = 0; k < nf; ++k) CHECK(isfinite(f[k])); } /* test_quantities_finite */
  { st_json *rep = NULL; CHECK(st_research_parameter_recovery(&rep, &e) == ST_E_OK);    /* test_output */
    const st_json *inv = st_json_get(rep, "duplication_invariance"), *br = st_json_get(rep, "branches");
    for (size_t i = 0; inv && i < inv->count; ++i) CHECK(inv->items[i]->boolean);
    CHECK(st_json_get(st_json_get(rep, "word_collision_control"), "same_braid_word")->boolean);
    CHECK(st_json_get(st_json_get(rep, "split"), "test")->ival == 70);
    for (size_t i = 0; br && i < br->count; ++i) {
      double cov = st_json_get(br->items[i], "interval_90_coverage")->num;
      CHECK(cov >= 0 && cov <= 1); CHECK(st_json_get(br->items[i], "interval_90_width_rad_s")->num > 0);
    }
    st_json_free(rep); }

  /* ---- graph_budget */
  const st_json *te0 = st_pr_split(&d, 2, 0)->records;
  CHECK(feats(te0, ST_PR_FULL_GRAPH, f, &nf) && nf == 55);                                /* test_fixed_graph_size */
  { st_json *dup = with_dup(te0); const st_pr_branch b[5] = {ST_PR_FULL_GRAPH, ST_PR_TEMPORAL, ST_PR_BRAID_WORD, ST_PR_BRAID_TIMED, ST_PR_BRAID_METRIC};
    for (int k = 0; k < 5; ++k) CHECK(same(te0, dup, b[k]));                             /* test_duplicate_invariance */
    for (int k = 0; k < 5; ++k) { CHECK(feats(te0, b[k], f, &nf)); for (size_t i = 0; i < nf; ++i) CHECK(isfinite(f[i])); } /* test_single_track_all_branches_finite */
    st_json_free(dup); }
  { st_json *r = st_json_clone(te0); *pos(r, 4, 0) += .1; CHECK(!same(te0, r, ST_PR_FULL_GRAPH)); st_json_free(r); } /* test_samples_change_graph */
  { st_json *r = st_json_clone(te0); *tim(r, 3) = *tim(r, 2);                            /* test_timestamp_order_rejected */
    CHECK(st_pr_features(r, ST_PR_FULL_GRAPH, f, &nf, &e) != ST_E_OK); st_json_free(r); }

  /* ---- source_fusion */
  st_sf_registry reg; CHECK(st_sf_load(root, &reg, &e) == ST_E_OK);
  st_pr_row sample = *st_pr_split(&d, 0, 0); sample.records = st_json_clone(sample.records);
  st_sf_metadata m;
  CHECK(st_sf_attached_metadata(&reg, &sample, &m, &e) == ST_E_OK);
  CHECK(st_sf_validate(&reg, &m, &e) == ST_E_OK);                                        /* test_source_provenance_registry */
  CHECK_STR(m.link_status, "synthetic_association");                                     /* test_synthetic_binding_explicit */
  { double a[64], b[64]; size_t na, nb; st_pr_row r2 = sample; r2.theta_rad_s += 1;      /* test_target_not_in_metadata */
    CHECK(st_sf_features(&reg, &sample, ST_SF_SOURCE_ONLY, a, &na, &e) == ST_E_OK && st_sf_features(&reg, &r2, ST_SF_SOURCE_ONLY, b, &nb, &e) == ST_E_OK);
    CHECK(na == nb && !memcmp(a, b, na * sizeof *a)); }
  { double a[64], b[64]; size_t na, nb; st_pr_row r2 = sample; r2.records = with_dup(sample.records); /* test_exact_duplicate_observation_invariant */
    CHECK(st_sf_features(&reg, &sample, ST_SF_TEMPORAL_SOURCE_BRAID, a, &na, &e) == ST_E_OK && st_sf_features(&reg, &r2, ST_SF_TEMPORAL_SOURCE_BRAID, b, &nb, &e) == ST_E_OK);
    CHECK(na == nb && na == 27 && !memcmp(a, b, na * sizeof *a)); st_json_free(r2.records); }
  { double a[64], b[64]; size_t na, nb; st_pr_row r2 = sample; r2.records = st_json_clone(sample.records); *pos(r2.records, 3, 0) += 0.5; /* test_change_measurement_detected */
    CHECK(st_sf_features(&reg, &sample, ST_SF_TEMPORAL_SOURCE, a, &na, &e) == ST_E_OK && st_sf_features(&reg, &r2, ST_SF_TEMPORAL_SOURCE, b, &nb, &e) == ST_E_OK);
    CHECK(memcmp(a, b, na * sizeof *a) != 0); st_json_free(r2.records); }
  { st_sf_metadata bad = m; snprintf(bad.source_url, sizeof bad.source_url, "https://fake.example"); CHECK(st_sf_validate(&reg, &bad, &e) == ST_E_SOURCE_POLICY); } /* test_bad_binding_rejected */
  { st_pr_collision c; CHECK(st_pr_collision_example(&c, &e) == ST_E_OK && c.same_braid_word); } /* test_known_braid_collision_retained */
  CHECK_STR(m.range_telemetry, "unknown"); CHECK_STR(m.corroboration, "unknown");          /* test_no_missing_as_false */
  st_json_free(sample.records); st_sf_free(&reg); st_pr_free(&d);
  return finish("research_parameter_recovery_graph_budget_source_fusion");
}
