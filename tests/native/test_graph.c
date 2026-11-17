// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/graph.h"
#include "check.h"

static const char *const L4[] = {"a", "b", "c", "d"};

static int paths_of(const st_csr *g, const char *const *lab, uint32_t root, size_t cap, char out[][16], size_t *np, st_code *rc) {
  uint32_t buf[256]; size_t nw; st_error e;
  *rc = st_graph_unfold_paths(g, lab, root, cap, buf, 256, np, &nw, &e);
  if (*rc) return 0;
  size_t p = 0, k = 0; out[0][0] = '\0';
  for (size_t i = 0; i < nw; ++i) { if (buf[i] == UINT32_MAX) { out[p][k] = '\0'; p++; k = 0; } else out[p][k++] = lab[buf[i]][0]; }
  return 1;
}

int main(int argc, char **argv) {
  const char *src = argc > 1 ? argv[1] : ".";
  st_error e; st_csr g; st_code rc;
  /* DAG-01: diamond unfolds to two paths; joins are not forced into a unique path. */
  st_edge diamond[] = {{0,1,ST_EDGE_DERIVED_FROM},{0,2,ST_EDGE_DERIVED_FROM},{1,3,ST_EDGE_DERIVED_FROM},{2,3,ST_EDGE_DERIVED_FROM}};
  CHECK(st_csr_build(&g, 4, diamond, 4, &e) == ST_E_OK);
  CHECK(g.out_row[4] == 4 && g.in_row[4] == 4);
  uint32_t order[4]; CHECK(st_graph_toposort(&g, L4, order, &e) == ST_E_OK);
  CHECK(order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3);
  char p[8][16]; size_t np;
  CHECK(paths_of(&g, L4, 0, 10, p, &np, &rc) && np == 2);
  CHECK_STR(p[0], "abd"); CHECK_STR(p[1], "acd");
  CHECK(!paths_of(&g, L4, 0, 1, p, &np, &rc) && rc == ST_E_BOUND);   /* path cap */
  CHECK(st_graph_containment_check(&g, &e) == ST_E_OK);               /* derived_from joins are legal */
  uint8_t mask[4]; size_t found;
  CHECK(st_graph_reach(&g, 0, 0, mask, 10, &found, &e) == ST_E_OK && found == 3 && mask[3]);
  CHECK(st_graph_reach(&g, 3, 1, mask, 10, &found, &e) == ST_E_OK && found == 3 && mask[0] && mask[1] && mask[2]); /* ancestry */
  CHECK(st_graph_reach(&g, 0, 0, mask, 2, &found, &e) == ST_E_BOUND);                                             /* bounded */
  CHECK(st_graph_reachable(&g, 1, 3) == 1 && st_graph_reachable(&g, 3, 1) == 0 && st_graph_reachable(&g, 1, 2) == 0);
  st_csr_free(&g);
  /* same diamond with containment joins violates the unique-parent forest rule */
  st_edge cdiamond[] = {{0,1,ST_EDGE_CONTAINS},{0,2,ST_EDGE_CONTAINS},{1,3,ST_EDGE_CONTAINS},{2,3,ST_EDGE_CONTAINS}};
  CHECK(st_csr_build(&g, 4, cdiamond, 4, &e) == ST_E_OK);
  CHECK(st_graph_containment_check(&g, &e) == ST_E_MULTIPLE_CONTAINMENT_PARENTS && e.index == 3);
  st_csr_free(&g);
  /* cycles */
  st_edge cyc[] = {{0,1,ST_EDGE_DERIVED_FROM},{1,0,ST_EDGE_DERIVED_FROM}};
  CHECK(st_csr_build(&g, 2, cyc, 2, &e) == ST_E_OK);
  CHECK(st_graph_toposort(&g, L4, order, &e) == ST_E_CYCLE);
  CHECK(!paths_of(&g, L4, 0, 10, p, &np, &rc) && rc == ST_E_CYCLE);
  CHECK(st_graph_reach(&g, 0, 0, mask, 10, &found, &e) == ST_E_OK && found == 1); /* traversal terminates on cycles */
  st_csr_free(&g);
  st_edge ccyc[] = {{0,1,ST_EDGE_CONTAINS},{1,2,ST_EDGE_CONTAINS},{2,0,ST_EDGE_CONTAINS}};
  CHECK(st_csr_build(&g, 3, ccyc, 3, &e) == ST_E_OK && st_graph_containment_check(&g, &e) == ST_E_CYCLE); st_csr_free(&g);
  /* malformed edges */
  st_edge self[] = {{1,1,ST_EDGE_DEPICTS}}; CHECK(st_csr_build(&g, 2, self, 1, &e) == ST_E_SELF_LOOP);
  st_edge dang[] = {{0,7,ST_EDGE_DEPICTS}}; CHECK(st_csr_build(&g, 2, dang, 1, &e) == ST_E_DANGLING);
  st_edge dup[] = {{0,1,ST_EDGE_DEPICTS},{0,1,ST_EDGE_DEPICTS}}; CHECK(st_csr_build(&g, 2, dup, 2, &e) == ST_E_DUPLICATE);
  st_edge multi[] = {{0,1,ST_EDGE_DEPICTS},{0,1,ST_EDGE_DERIVED_FROM}}; CHECK(st_csr_build(&g, 2, multi, 2, &e) == ST_E_OK); st_csr_free(&g);
  /* canonical digest: invariant to node and edge order, sensitive to any edge change */
  const char *la[] = {"archive", "case", "record-a", "record-b"}, *lb[] = {"record-b", "record-a", "case", "archive"};
  st_edge ea[] = {{0,1,ST_EDGE_CONTAINS},{1,2,ST_EDGE_CONTAINS},{1,3,ST_EDGE_DERIVED_FROM},{2,3,ST_EDGE_DERIVED_FROM}};
  st_edge eb[] = {{1,0,ST_EDGE_DERIVED_FROM},{2,0,ST_EDGE_DERIVED_FROM},{2,1,ST_EDGE_CONTAINS},{3,2,ST_EDGE_CONTAINS}};
  char h1[65], h2[65], h3[65];
  CHECK(st_graph_canonical_digest(la, 4, ea, 4, h1, &e) == ST_E_OK);
  CHECK(st_graph_canonical_digest(lb, 4, eb, 4, h2, &e) == ST_E_OK);
  CHECK_STR(h1, h2);
  ea[3].type = ST_EDGE_DEPICTS; CHECK(st_graph_canonical_digest(la, 4, ea, 4, h3, &e) == ST_E_OK); CHECK(strcmp(h1, h3) != 0);
  const char *ldup[] = {"x", "x"}; CHECK(st_graph_canonical_digest(ldup, 2, NULL, 0, h3, &e) == ST_E_DUPLICATE);
  /* provenance deduplication */
  char d[5][65] = {"bb", "aa", "bb", "cc", "aa"};
  CHECK(st_dedup_digests(d, 5) == 3); CHECK_STR(d[0], "aa"); CHECK_STR(d[2], "cc");
  /* record sets: fixture loads; adversarial variants fail with machine-readable codes */
  char path[4200]; snprintf(path, sizeof path, "%s/fixtures/archives/synthetic-batch.json", src);
  st_json *doc = st_json_parse_file(path, &e); CHECK(doc != NULL);
  st_record_set rs; CHECK(st_records_load(&rs, doc, &e) == ST_E_OK);
  CHECK(rs.n == 3 && rs.contains.m == 2);
  CHECK(rs.rec[0].range_telemetry == ST_UNKNOWN && rs.rec[2].range_telemetry == ST_KNOWN_FALSE);
  CHECK(rs.rec[1].range_telemetry == ST_UNKNOWN);      /* absent key = unknown, never false */
  CHECK(isnan(rs.rec[2].timestamp_uncertainty_ms));    /* missing uncertainty is not zero */
  st_records_free(&rs); st_json_free(doc);
  const char *bad[][2] = {
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"parent_id\":\"b\",\"release_epoch\":\"1\",\"mime\":\"x/y\"},"
     "{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"b\",\"parent_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\"}]}", "E_CYCLE"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"parent_id\":\"zz\",\"release_epoch\":\"1\",\"mime\":\"x/y\"}]}", "E_DANGLING"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\"},"
     "{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\"}]}", "E_DUPLICATE"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"bad\"}]}", "E_SCHEMA"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\",\"corroborated\":\"yes\"}]}", "E_SCHEMA"},
    {"{\"records\":[{\"source\":\"myspace\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\"}]}", "E_SOURCE_POLICY"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\" \",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\"}]}", "E_SCHEMA"},
    {"{\"records\":[{\"source\":\"pursue\",\"agency\":\"A\",\"canonical_id\":\"a\",\"release_epoch\":\"1\",\"mime\":\"x/y\",\"evidence_sha256\":\"00\"}]}", "E_DIGEST"},
  };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) {
    st_json *dj = st_json_parse(bad[i][0], strlen(bad[i][0]), &e);
    st_code c = st_records_load(&rs, dj, &e);
    CHECK_STR(st_code_name(c), bad[i][1]);
    st_json_free(dj);
  }
  return finish("test_graph");
}
