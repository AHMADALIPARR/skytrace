// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/graph.h"
#include <stdlib.h>
#include <string.h>

static const char *const EDGE_NAMES[4] = {"contains", "derived_from", "depicts", "calibrated_by"};
int st_edge_type_parse(const char *s, size_t n, st_edge_type *out) {
  for (int i = 0; i < 4; ++i) if (strlen(EDGE_NAMES[i]) == n && memcmp(s, EDGE_NAMES[i], n) == 0) { *out = (st_edge_type)i; return 1; }
  return 0;
}
const char *st_edge_type_name(st_edge_type t) { return (unsigned)t < 4 ? EDGE_NAMES[t] : "?"; }

void st_csr_free(st_csr *g) {
  free(g->out_row); free(g->out_col); free(g->out_type); free(g->in_row); free(g->in_col); free(g->in_type);
  memset(g, 0, sizeof *g);
}

typedef struct { uint32_t a, b; uint8_t t; } trip;
static int cmp_trip(const void *x, const void *y) {
  const trip *p = x, *q = y;
  if (p->a != q->a) return p->a < q->a ? -1 : 1;
  if (p->b != q->b) return p->b < q->b ? -1 : 1;
  return (p->t > q->t) - (p->t < q->t);
}

static st_code fill(size_t n, trip *t, size_t m, size_t **row, uint32_t **col, uint8_t **ty, st_error *err) {
  qsort(t, m, sizeof *t, cmp_trip);
  *row = calloc(n + 1, sizeof **row); *col = malloc((m ? m : 1) * sizeof **col); *ty = malloc(m ? m : 1);
  if (!*row || !*col || !*ty) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  for (size_t i = 0; i < m; ++i) (*row)[t[i].a + 1]++;
  for (size_t i = 0; i < n; ++i) (*row)[i + 1] += (*row)[i];
  for (size_t i = 0; i < m; ++i) { (*col)[i] = t[i].b; (*ty)[i] = t[i].t; }
  return ST_E_OK;
}

st_code st_csr_build(st_csr *g, size_t n, const st_edge *e, size_t m, st_error *err) {
  memset(g, 0, sizeof *g);
  if (n > ST_GRAPH_MAX_NODES || m > ST_GRAPH_MAX_EDGES) return st_fail(err, ST_E_SIZE_LIMIT, -1, "graph exceeds bounds");
  trip *t = malloc((m ? m : 1) * sizeof *t);
  if (!t) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  for (size_t i = 0; i < m; ++i) {
    if (e[i].from >= n || e[i].to >= n || (unsigned)e[i].type > 3) { free(t); return st_fail(err, ST_E_DANGLING, (long)i, "edge %zu references a missing node", i); }
    if (e[i].from == e[i].to) { free(t); return st_fail(err, ST_E_SELF_LOOP, (long)i, "edge %zu is a self loop", i); }
    t[i].a = e[i].from; t[i].b = e[i].to; t[i].t = (uint8_t)e[i].type;
  }
  g->n = n; g->m = m;
  st_code rc = fill(n, t, m, &g->out_row, &g->out_col, &g->out_type, err);
  for (size_t i = 1; !rc && i < m; ++i)
    if (t[i].a == t[i-1].a && t[i].b == t[i-1].b && t[i].t == t[i-1].t) rc = st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate edge");
  if (!rc) {
    for (size_t i = 0; i < m; ++i) { uint32_t x = t[i].a; t[i].a = t[i].b; t[i].b = x; }
    rc = fill(n, t, m, &g->in_row, &g->in_col, &g->in_type, err);
  }
  free(t);
  if (rc) st_csr_free(g);
  return rc;
}

/* Binary min-heap of node ids ordered by label. */
typedef struct { uint32_t *a; size_t n; const char *const *lab; } heap;
static int less(const heap *h, uint32_t x, uint32_t y) {
  if (!h->lab) return x < y;
  int c = strcmp(h->lab[x], h->lab[y]); return c < 0 || (c == 0 && x < y);
}
static void hpush(heap *h, uint32_t v) {
  size_t i = h->n++; h->a[i] = v;
  while (i && less(h, h->a[i], h->a[(i-1)/2])) { uint32_t t = h->a[i]; h->a[i] = h->a[(i-1)/2]; h->a[(i-1)/2] = t; i = (i-1)/2; }
}
static uint32_t hpop(heap *h) {
  uint32_t top = h->a[0]; h->a[0] = h->a[--h->n]; size_t i = 0;
  while (1) {
    size_t l = 2*i+1, r = l+1, s = i;
    if (l < h->n && less(h, h->a[l], h->a[s])) s = l;
    if (r < h->n && less(h, h->a[r], h->a[s])) s = r;
    if (s == i) break;
    uint32_t t = h->a[i]; h->a[i] = h->a[s]; h->a[s] = t; i = s;
  }
  return top;
}

st_code st_graph_toposort(const st_csr *g, const char *const *labels, uint32_t *order, st_error *err) {
  size_t *deg = calloc(g->n + 1, sizeof *deg); heap h = { malloc((g->n + 1) * sizeof(uint32_t)), 0, labels };
  if (!deg || !h.a) { free(deg); free(h.a); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  for (size_t v = 0; v < g->n; ++v) deg[v] = g->in_row[v+1] - g->in_row[v];
  for (size_t v = 0; v < g->n; ++v) if (!deg[v]) hpush(&h, (uint32_t)v);
  size_t k = 0;
  while (h.n) {
    uint32_t u = hpop(&h); order[k++] = u;
    for (size_t j = g->out_row[u]; j < g->out_row[u+1]; ++j) if (--deg[g->out_col[j]] == 0) hpush(&h, g->out_col[j]);
  }
  st_code rc = ST_E_OK;
  if (k != g->n) {
    long bad = -1; for (size_t v = 0; v < g->n; ++v) if (deg[v]) { bad = (long)v; break; }
    rc = st_fail(err, ST_E_CYCLE, bad, "E_CYCLE: %zu of %zu nodes lie on or behind a directed cycle", g->n - k, g->n);
  }
  free(deg); free(h.a); return rc;
}

st_code st_graph_containment_check(const st_csr *g, st_error *err) {
  int *parent = malloc((g->n ? g->n : 1) * sizeof *parent);
  if (!parent) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  for (size_t v = 0; v < g->n; ++v) {
    parent[v] = -1;
    for (size_t j = g->in_row[v]; j < g->in_row[v+1]; ++j) if (g->in_type[j] == ST_EDGE_CONTAINS) {
      if (parent[v] != -1) { free(parent); return st_fail(err, ST_E_MULTIPLE_CONTAINMENT_PARENTS, (long)v, "E_MULTIPLE_CONTAINMENT_PARENTS at node %zu", v); }
      parent[v] = (int)g->in_col[j];
    }
  }
  /* Functional parent graph: more than n parent steps from any node implies a cycle. */
  for (size_t v = 0; v < g->n; ++v) {
    int cur = (int)v; size_t steps = 0;
    while (cur != -1) {
      if (steps++ > g->n) { free(parent); return st_fail(err, ST_E_CYCLE, (long)v, "containment cycle through node %zu", v); }
      cur = parent[cur];
    }
  }
  free(parent); return ST_E_OK;
}

st_code st_graph_reach(const st_csr *g, uint32_t start, int anc, uint8_t *mask, size_t max_visits, size_t *found, st_error *err) {
  if (start >= g->n) return st_fail(err, ST_E_ARGUMENT, (long)start, "start node out of range");
  memset(mask, 0, g->n);
  uint32_t *q = malloc((g->n ? g->n : 1) * sizeof *q);
  if (!q) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  size_t head = 0, tail = 0, visits = 0; q[tail++] = start;
  const size_t *row = anc ? g->in_row : g->out_row; const uint32_t *col = anc ? g->in_col : g->out_col;
  while (head < tail) {
    uint32_t u = q[head++];
    for (size_t j = row[u]; j < row[u+1]; ++j) {
      uint32_t v = col[j];
      if (v == start || mask[v]) continue;
      if (++visits > max_visits) { free(q); return st_fail(err, ST_E_BOUND, (long)v, "E_BOUND: traversal exceeded %zu visits", max_visits); }
      mask[v] = 1; q[tail++] = v;
    }
  }
  free(q); *found = visits; return ST_E_OK;
}

int st_graph_reachable(const st_csr *g, uint32_t from, uint32_t to) {
  if (from >= g->n || to >= g->n) return -1;
  uint8_t *mask = malloc(g->n ? g->n : 1); size_t f; st_error e;
  if (!mask) return -1;
  int r = st_graph_reach(g, from, 0, mask, g->n, &f, &e) ? -1 : mask[to];
  free(mask); return r;
}

st_code st_graph_unfold_paths(const st_csr *g, const char *const *labels, uint32_t root, size_t max_paths,
                              uint32_t *out, size_t cap, size_t *np, size_t *nw, st_error *err) {
  if (root >= g->n || max_paths < 1) return st_fail(err, ST_E_ARGUMENT, -1, "invalid root or path cap");
  uint32_t *order = malloc((g->n ? g->n : 1) * sizeof *order);
  if (!order) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  st_code rc = st_graph_toposort(g, labels, order, err); free(order);
  if (rc) return rc;
  /* Iterative DFS with explicit stack of (node, next child slot); children visited by label order. */
  size_t depth_cap = g->n + 1;
  uint32_t *stack = malloc(depth_cap * sizeof *stack); size_t *slot = malloc(depth_cap * sizeof *slot);
  uint32_t *kids = malloc((g->m ? g->m : 1) * sizeof *kids);
  if (!stack || !slot || !kids) { free(stack); free(slot); free(kids); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  /* Sort each adjacency segment by label once. */
  for (size_t u = 0; u < g->n; ++u) {
    size_t a = g->out_row[u], b = g->out_row[u+1];
    for (size_t i = a; i < b; ++i) kids[i] = g->out_col[i];
    for (size_t i = a + 1; i < b; ++i) { uint32_t x = kids[i]; size_t j = i;
      while (j > a && (labels ? strcmp(labels[x], labels[kids[j-1]]) < 0 : x < kids[j-1])) { kids[j] = kids[j-1]; j--; } kids[j] = x; }
  }
  size_t sp = 0, paths = 0, w = 0; stack[sp] = root; slot[sp] = g->out_row[root]; sp++;
  while (sp && !rc) {
    uint32_t u = stack[sp-1];
    if (g->out_row[u] == g->out_row[u+1]) {           /* leaf: emit path */
      if (++paths > max_paths) { rc = st_fail(err, ST_E_BOUND, -1, "path cap exceeded"); break; }
      if (w + sp + 1 > cap) { rc = st_fail(err, ST_E_BOUND, -1, "path output capacity exceeded"); break; }
      for (size_t i = 0; i < sp; ++i) out[w++] = stack[i];
      out[w++] = UINT32_MAX; sp--; continue;
    }
    if (slot[sp-1] == g->out_row[u+1]) { sp--; continue; }
    uint32_t v = kids[slot[sp-1]++];
    stack[sp] = v; slot[sp] = g->out_row[v]; sp++;
  }
  free(stack); free(slot); free(kids);
  if (!rc) { *np = paths; *nw = w; }
  return rc;
}

static int cmp_cstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

st_code st_graph_canonical_digest(const char *const *labels, size_t n, const st_edge *e, size_t m, char hex[65], st_error *err) {
  const char **sorted = malloc((n ? n : 1) * sizeof *sorted);
  char **lines = malloc((m ? m : 1) * sizeof *lines);
  if (!sorted || !lines) { free(sorted); free(lines); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  memcpy(sorted, labels, n * sizeof *sorted);
  qsort(sorted, n, sizeof *sorted, cmp_cstr);
  st_code rc = ST_E_OK;
  for (size_t i = 1; i < n; ++i) if (strcmp(sorted[i], sorted[i-1]) == 0) { rc = st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate node label %s", sorted[i]); break; }
  size_t made = 0;
  for (size_t i = 0; !rc && i < m; ++i) {
    if (e[i].from >= n || e[i].to >= n) { rc = st_fail(err, ST_E_DANGLING, (long)i, "dangling edge"); break; }
    st_buf b; st_buf_init(&b);
    st_buf_printf(&b, "%s\x1f%s\x1f%s\n", labels[e[i].from], labels[e[i].to], st_edge_type_name(e[i].type));
    lines[made++] = b.data;
  }
  if (!rc) {
    qsort(lines, made, sizeof *lines, cmp_cstr);
    st_sha256_ctx c; st_sha256_init(&c);
    st_sha256_update(&c, "skytrace-graph-v1\n", 18);
    for (size_t i = 0; i < n; ++i) { st_sha256_update(&c, sorted[i], strlen(sorted[i])); st_sha256_update(&c, "\n", 1); }
    st_sha256_update(&c, "--\n", 3);
    for (size_t i = 0; i < made; ++i) {
      if (i && strcmp(lines[i], lines[i-1]) == 0) { rc = st_fail(err, ST_E_DUPLICATE, (long)i, "duplicate edge"); break; }
      st_sha256_update(&c, lines[i], strlen(lines[i]));
    }
    uint8_t d[32]; st_sha256_final(&c, d); st_hex_encode(d, 32, hex);
  }
  for (size_t i = 0; i < made; ++i) free(lines[i]);
  free(lines); free(sorted); return rc;
}

static int cmp_hex(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
size_t st_dedup_digests(char (*d)[65], size_t n) {
  if (!n) return 0;
  qsort(d, n, sizeof *d, cmp_hex);
  size_t k = 1;
  for (size_t i = 1; i < n; ++i) if (strcmp(d[i], d[k-1]) != 0) { if (k != i) memcpy(d[k], d[i], 65); k++; }
  return k;
}
