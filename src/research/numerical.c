// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Numerical audit of archive closure + DAG + braid on explicitly synthetic records (port of
 * research/numerical/analyze.py). No observational UAP claims are derived from fixture-only data. */
#include "skytrace/research.h"
#include "skytrace/braid.h"
#include <stdio.h>
#include <string.h>

/* Matches st_active in kernel/src/skytrace.c: resolution = 1 blocks; -1 does not. */
int st_num_kernel_active(int range, int corroborated, int resolution) { return resolution != 1 && (range != 1 || corroborated != 1); }
/* Conservative publication policy: unknown resolution is never auto-admitted. */
int st_num_strict_active(int range, int corroborated, int resolution) { return resolution == 0 && (range != 1 || corroborated != 1); }

static st_code bool3(const st_json *x, int *v, st_error *err) {
  if (!x || x->type == ST_JNULL) { *v = -1; return ST_E_OK; }
  if (x->type != ST_JBOOL) return st_fail(err, ST_E_SCHEMA, -1, "expected bool or null");
  *v = x->boolean ? 1 : 0; return ST_E_OK;
}

st_code st_num_metrics(const st_json *records, st_json **out, st_error *err) {
  *out = NULL;
  if (!records || records->type != ST_JARR) return st_fail(err, ST_E_SCHEMA, -1, "records must be an array");
  size_t n = records->count;
  if (n > ST_SM_MAX_NODES) return st_fail(err, ST_E_BOUND, -1, "too many records");
  const char *ids[ST_SM_MAX_NODES] = {0}; const char *edges[ST_SM_MAX_NODES][2] = {{0}}; size_t ne = 0;
  for (size_t i = 0; i < n; ++i) {
    const st_json *c = st_json_get(records->items[i], "canonical_id");
    if (!c || c->type != ST_JSTR) return st_fail(err, ST_E_SCHEMA, (long)i, "canonical_id must be a string");
    ids[i] = c->str;
  }
  for (size_t i = 0; i < n; ++i) {
    const st_json *p = st_json_get(records->items[i], "parent_id");
    if (p && p->type == ST_JSTR && p->str[0]) { edges[ne][0] = p->str; edges[ne][1] = ids[i]; ne++; }
  }
  size_t order[ST_SM_MAX_NODES];
  st_code rc = st_sm_validate_dag(ids, n, (const char *const (*)[2])edges, ne, order, err);
  if (rc) return rc;
  size_t roots[ST_SM_MAX_NODES], nr = 0, npaths = 0, maxlen = 0;
  for (size_t k = 0; k < n; ++k) {
    int incoming = 0;
    for (size_t e = 0; e < ne; ++e) incoming |= !strcmp(edges[e][1], ids[order[k]]);
    if (!incoming) roots[nr++] = order[k];
  }
  for (size_t r = 0; r < nr; ++r) {
    size_t buf[4096], np = 0;
    if ((rc = st_sm_unfold_paths(ids, n, (const char *const (*)[2])edges, ne, ids[roots[r]], 10000, buf, 4096, &np, err))) return rc;
    size_t len = 0;
    for (size_t i = 0, seen = 0; seen < np; ++i) {
      if (buf[i] == (size_t)-1) { if (len - 1 > maxlen) maxlen = len - 1; len = 0; seen++; } else len++;
    }
    npaths += np;
  }
  long counts[7] = {0};   /* kernel_active, kernel_inactive, strict_active, strict_inactive, unknown_{corroboration,range,resolution} */
  for (size_t i = 0; i < n; ++i) {
    int r = -1, c = -1, s = -1; const st_json *rec = records->items[i];
    if ((rc = bool3(st_json_get(rec, "has_range_telemetry"), &r, err)) || (rc = bool3(st_json_get(rec, "corroborated"), &c, err)) ||
        (rc = bool3(st_json_get(rec, "resolved"), &s, err))) return rc;
    counts[st_num_kernel_active(r, c, s) ? 0 : 1]++;
    counts[st_num_strict_active(r, c, s) ? 2 : 3]++;
    if (s == -1) counts[6]++;
    if (r == -1) counts[5]++;
    if (c == -1) counts[4]++;
  }
  static const char *CK[7] = {"kernel_active", "kernel_inactive", "strict_active", "strict_inactive", "unknown_corroboration", "unknown_range", "unknown_resolution"};
  st_json *o = st_json_object(), *cl = st_json_object(), *to = st_json_array();
  for (int k = 0; k < 7; ++k) if (counts[k]) st_json_set(cl, CK[k], st_json_int(counts[k]));   /* Counter keeps only seen keys */
  for (size_t k = 0; k < n; ++k) st_json_push(to, st_json_string(ids[order[k]]));
  st_json_set(o, "nodes", st_json_int((int64_t)n));
  st_json_set(o, "edges", st_json_int((int64_t)ne));
  st_json_set(o, "roots", st_json_int((int64_t)nr));
  st_json_set(o, "root_to_leaf_paths", st_json_int((int64_t)npaths));
  st_json_set(o, "max_path_edges", st_json_int((int64_t)maxlen));
  st_json_set(o, "topological_order", to);
  st_json_set(o, "closure", cl);
  *out = o;
  return ST_E_OK;
}

st_json *st_num_truth_table(void) {
  int ka = 0, sa = 0, nm = 0; st_json *mm = st_json_array();
  for (int r = -1; r <= 1; ++r) for (int c = -1; c <= 1; ++c) for (int s = -1; s <= 1; ++s) {
    int k = st_num_kernel_active(r, c, s), q = st_num_strict_active(r, c, s);
    ka += k; sa += q;
    if (k != q) {
      st_json *m = st_json_object(); nm++;
      st_json_set(m, "range", st_json_int(r)); st_json_set(m, "corroborated", st_json_int(c)); st_json_set(m, "resolution", st_json_int(s));
      st_json_push(mm, m);
    }
  }
  st_json *o = st_json_object();
  st_json_set(o, "ternary_combinations", st_json_int(27));
  st_json_set(o, "kernel_active_combinations", st_json_int(ka));
  st_json_set(o, "strict_active_combinations", st_json_int(sa));
  st_json_set(o, "policy_mismatch_combinations", st_json_int(nm));
  st_json_set(o, "mismatches", mm);
  return o;
}

st_code st_num_braid_audit(st_json **out, st_error *err) {
  st_mat2 a, b, w1, w2; st_fib_generators(&a, &b);
  const int x[3] = {1, 2, 1}, y[3] = {2, 1, 2};
  st_code rc = st_braid_word_matrix(x, 3, &w1, err);
  if (!rc) rc = st_braid_word_matrix(y, 3, &w2, err);
  if (rc) return rc;
  st_json *o = st_json_object(), *dw = st_json_array(), *ew = st_json_array();
  for (int i = 0; i < 3; ++i) { st_json_push(dw, st_json_int(x[i])); st_json_push(ew, st_json_int(y[i])); }
  st_json_set(o, "braid_relation_max_abs_error", st_json_double(st_mat2_max_error(st_mat2_mul(st_mat2_mul(a, b), a), st_mat2_mul(st_mat2_mul(b, a), b))));
  st_json_set(o, "dag_word_example", dw);
  st_json_set(o, "equivalent_word", ew);
  st_json_set(o, "word_matrix_max_abs_error", st_json_double(st_mat2_max_error(w1, w2)));
  st_json_set(o, "mapping_status", st_json_string("example braid words, not uniquely inferred from provenance"));
  *out = o;
  return ST_E_OK;
}

st_code st_research_numerical(const char *root, st_json **out, st_error *err) {
  char path[4096]; snprintf(path, sizeof path, "%s/fixtures/archives/synthetic-batch.json", root);
  st_json *raw = st_json_parse_file(path, err);
  if (!raw) return err->code ? err->code : ST_E_IO;
  st_json *g = NULL, *br = NULL;
  st_code rc = st_num_metrics(st_json_get(raw, "records"), &g, err);
  if (!rc) rc = st_num_braid_audit(&br, err);
  st_json_free(raw);
  if (rc) { st_json_free(g); return rc; }
  st_json *o = st_json_object();
  st_json_set(o, "fixture", st_json_string("synthetic-batch.json"));
  st_json_set(o, "observational_data", st_json_bool(0));
  st_json_set(o, "graph", g);
  st_json_set(o, "ternary_closure", st_num_truth_table());
  st_json_set(o, "braid", br);
  *out = o;
  return ST_E_OK;
}
