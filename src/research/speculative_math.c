// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* SKYTRACE speculative math (port of research/speculative_math/core.py): finite classical models,
 * not physical probe detection. The Fibonacci braid matrices live in src/braid/fibonacci.c. */
#include "skytrace/research.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static long find(const char *const *nodes, size_t nn, const char *x) {
  for (size_t i = 0; i < nn; ++i) if (!strcmp(nodes[i], x)) return (long)i;
  return -1;
}

st_code st_sm_validate_dag(const char *const *nodes, size_t nn, const char *const (*edges)[2], size_t ne,
                           size_t *order, st_error *err) {
  if (nn > ST_SM_MAX_NODES) return st_fail(err, ST_E_BOUND, -1, "too many nodes");
  for (size_t i = 0; i < nn; ++i) for (size_t j = 0; j < i; ++j) if (!strcmp(nodes[i], nodes[j])) return st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate node");
  for (size_t i = 0; i < ne; ++i) for (size_t j = 0; j < i; ++j)
    if (!strcmp(edges[i][0], edges[j][0]) && !strcmp(edges[i][1], edges[j][1])) return st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate edge");
  int indeg[ST_SM_MAX_NODES] = {0};
  for (size_t i = 0; i < ne; ++i) {
    long u = find(nodes, nn, edges[i][0]), v = find(nodes, nn, edges[i][1]);
    if (u < 0 || v < 0) return st_fail(err, ST_E_DANGLING, (long)i, "dangling edge");
    indeg[v]++;
  }
  int done[ST_SM_MAX_NODES] = {0}, avail[ST_SM_MAX_NODES] = {0}; size_t k = 0;
  for (size_t i = 0; i < nn; ++i) avail[i] = indeg[i] == 0;
  for (;;) {
    long best = -1;                      /* smallest available name */
    for (size_t i = 0; i < nn; ++i) if (avail[i] && !done[i] && (best < 0 || strcmp(nodes[i], nodes[best]) < 0)) best = (long)i;
    if (best < 0) break;
    done[best] = 1; avail[best] = 0; order[k++] = (size_t)best;
    for (size_t e = 0; e < ne; ++e)
      if (!strcmp(edges[e][0], nodes[best])) { long v = find(nodes, nn, edges[e][1]); if (--indeg[v] == 0) avail[v] = 1; }
  }
  if (k != nn) return st_fail(err, ST_E_CYCLE, -1, "cycle");
  return ST_E_OK;
}

typedef struct { size_t len; size_t node[ST_SM_MAX_NODES]; } path_t;
static const char *const *g_nodes;
static int cmp_path(const void *a, const void *b) {
  const path_t *p = a, *q = b;
  for (size_t i = 0; i < p->len && i < q->len; ++i) { int c = strcmp(g_nodes[p->node[i]], g_nodes[q->node[i]]); if (c) return c; }
  return (p->len > q->len) - (p->len < q->len);
}
static int cmp_name_desc(const void *a, const void *b) { return -strcmp(g_nodes[*(const size_t *)a], g_nodes[*(const size_t *)b]); }

st_code st_sm_unfold_paths(const char *const *nodes, size_t nn, const char *const (*edges)[2], size_t ne,
                           const char *root, size_t max_paths, size_t *out, size_t cap, size_t *npaths, st_error *err) {
  size_t order[ST_SM_MAX_NODES];
  st_code rc = st_sm_validate_dag(nodes, nn, edges, ne, order, err);
  if (rc) return rc;
  long r = find(nodes, nn, root);
  if (r < 0 || max_paths < 1) return st_fail(err, ST_E_ARGUMENT, -1, "invalid root or path cap");
  size_t room = max_paths + nn + 2;
  path_t *pending = malloc(room * sizeof *pending), *paths = malloc((max_paths + 1) * sizeof *paths);
  if (!pending || !paths) { free(pending); free(paths); return st_fail(err, ST_E_NOMEM, -1, "unfold"); }
  size_t np = 1, nf = 0;
  pending[0].len = 1; pending[0].node[0] = (size_t)r;
  g_nodes = nodes;
  while (np) {
    path_t p = pending[--np];
    size_t ch[ST_SM_MAX_NODES], nc = 0, last = p.node[p.len - 1];
    for (size_t e = 0; e < ne; ++e) if (!strcmp(edges[e][0], nodes[last])) ch[nc++] = (size_t)find(nodes, nn, edges[e][1]);
    qsort(ch, nc, sizeof *ch, cmp_name_desc);
    if (!nc) {
      paths[nf++] = p;
      if (nf > max_paths) { free(pending); free(paths); return st_fail(err, ST_E_OVERFLOW, -1, "path cap exceeded"); }
    } else {
      if (np + nc + nf > max_paths) { free(pending); free(paths); return st_fail(err, ST_E_OVERFLOW, -1, "path cap exceeded"); }
      for (size_t c = 0; c < nc; ++c) { path_t q = p; q.node[q.len++] = ch[c]; pending[np++] = q; }
    }
  }
  qsort(paths, nf, sizeof *paths, cmp_path);
  size_t k = 0;
  for (size_t i = 0; i < nf; ++i) {
    if (k + paths[i].len + 1 > cap) { free(pending); free(paths); return st_fail(err, ST_E_BOUND, -1, "output buffer too small"); }
    for (size_t j = 0; j < paths[i].len; ++j) out[k++] = paths[i].node[j];
    out[k++] = (size_t)-1;
  }
  *npaths = nf;
  free(pending); free(paths);
  return ST_E_OK;
}

st_code st_sm_parity_encode(const int *bits, size_t n, int *out, st_error *err) {
  if (!n) return st_fail(err, ST_E_ARGUMENT, -1, "bits must be a nonempty binary sequence");
  int s = 0;
  for (size_t i = 0; i < n; ++i) { if (bits[i] != 0 && bits[i] != 1) return st_fail(err, ST_E_ARGUMENT, (long)i, "bits must be a nonempty binary sequence"); out[i] = bits[i]; s += bits[i]; }
  out[n] = s % 2;
  return ST_E_OK;
}
st_code st_sm_parity_syndrome(const int *code, size_t n, int *syndrome, st_error *err) {
  if (n < 2) return st_fail(err, ST_E_ARGUMENT, -1, "invalid codeword");
  int s = 0;
  for (size_t i = 0; i < n; ++i) { if (code[i] != 0 && code[i] != 1) return st_fail(err, ST_E_ARGUMENT, (long)i, "invalid codeword"); s += code[i]; }
  *syndrome = s % 2;
  return ST_E_OK;
}

st_code st_sm_replicate_probes(long initial, long resources, long cost, long branching, long generations,
                               st_sm_probe *states, size_t cap, size_t *n, st_error *err) {
  if (initial < 1 || resources < 0 || cost < 1 || branching < 0 || generations < 0) return st_fail(err, ST_E_ARGUMENT, -1, "invalid replication parameters");
  if ((size_t)generations + 1 > cap) return st_fail(err, ST_E_BOUND, -1, "state buffer too small");
  long count = initial, remaining = resources;
  states[0] = (st_sm_probe){0, initial, resources};
  for (long g = 1; g <= generations; ++g) {
    if (branching && count > LONG_MAX / branching) return st_fail(err, ST_E_OVERFLOW, g, "offspring count overflows");
    long want = count * branching, afford = remaining / cost, off = want < afford ? want : afford;
    remaining -= off * cost; count += off;
    states[g] = (st_sm_probe){g, count, remaining};
  }
  *n = (size_t)generations + 1;
  return ST_E_OK;
}

static int cmps(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
static int cmpe(const void *a, const void *b) {
  const char *const *x = a, *const *y = b; int c = strcmp(x[0], y[0]); return c ? c : strcmp(x[1], y[1]);
}
st_code st_sm_bridge_edges(const char *const *loc, size_t nl, const char *const (*br)[2], size_t nb, st_json **out, st_error *err) {
  for (size_t i = 0; i < nl; ++i) for (size_t j = 0; j < i; ++j) if (!strcmp(loc[i], loc[j])) return st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate location");
  for (size_t i = 0; i < nb; ++i)
    if (find(loc, nl, br[i][0]) < 0 || find(loc, nl, br[i][1]) < 0 || !strcmp(br[i][0], br[i][1])) return st_fail(err, ST_E_ARGUMENT, (long)i, "invalid hypothetical bridge endpoint");
  const char **v = malloc((nl + 1) * sizeof *v); const char *(*e)[2] = malloc((nb + 1) * sizeof *e);
  memcpy(v, loc, nl * sizeof *v); memcpy(e, br, nb * sizeof *e);
  qsort(v, nl, sizeof *v, cmps); qsort(e, nb, sizeof *e, cmpe);
  st_json *o = st_json_object(), *vs = st_json_array(), *es = st_json_array();
  for (size_t i = 0; i < nl; ++i) st_json_push(vs, st_json_string(v[i]));
  for (size_t i = 0; i < nb; ++i) { st_json *p = st_json_array(); st_json_push(p, st_json_string(e[i][0])); st_json_push(p, st_json_string(e[i][1])); st_json_push(es, p); }
  st_json_set(o, "status", st_json_string("speculative_not_observed"));
  st_json_set(o, "vertices", vs); st_json_set(o, "edges", es);
  free(v); free(e);
  *out = o;
  return ST_E_OK;
}
