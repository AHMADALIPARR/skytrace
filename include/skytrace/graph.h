#ifndef SKYTRACE_GRAPH_H
#define SKYTRACE_GRAPH_H
/* Provenance graph in compressed sparse row form.
 *  - The provenance DAG (derived_from, depicts, calibrated_by) may have many incoming edges.
 *  - The containment forest (contains) must give each node at most one parent.
 * The two are checked separately and never conflated: a DAG does not imply unique paths.
 * Every traversal takes an explicit bound and fails with E_BOUND instead of running away. */
#include <stddef.h>
#include <stdint.h>
#include "skytrace/core.h"

typedef enum { ST_EDGE_CONTAINS = 0, ST_EDGE_DERIVED_FROM = 1, ST_EDGE_DEPICTS = 2, ST_EDGE_CALIBRATED_BY = 3 } st_edge_type;
int st_edge_type_parse(const char *s, size_t n, st_edge_type *out);
const char *st_edge_type_name(st_edge_type t);

typedef struct { uint32_t from, to; st_edge_type type; } st_edge;

typedef struct {
  size_t n, m;
  size_t *out_row; uint32_t *out_col; uint8_t *out_type;  /* outgoing CSR, columns sorted */
  size_t *in_row;  uint32_t *in_col;  uint8_t *in_type;   /* incoming CSR, columns sorted */
} st_csr;

#define ST_GRAPH_MAX_NODES 1000000u
#define ST_GRAPH_MAX_EDGES 10000000u

/* Rejects dangling endpoints (E_DANGLING), self loops (E_SELF_LOOP), duplicate (from,to,type) (E_DUPLICATE). */
st_code st_csr_build(st_csr *g, size_t n, const st_edge *edges, size_t m, st_error *err);
void st_csr_free(st_csr *g);

/* Deterministic Kahn ordering: among ready nodes, the smallest label (byte order) first.
 * Fails with E_CYCLE (err->index = a node on a cycle) when the graph is not acyclic. */
st_code st_graph_toposort(const st_csr *g, const char *const *labels, uint32_t *order, st_error *err);
/* Containment forest: at most one `contains` parent per node, and acyclic. */
st_code st_graph_containment_check(const st_csr *g, st_error *err);
/* Descendants (reach) or ancestors (ancestry) of `start`, excluding start, as a 0/1 mask.
 * Visits at most max_visits nodes; returns count in *found. */
st_code st_graph_reach(const st_csr *g, uint32_t start, int ancestors, uint8_t *mask, size_t max_visits, size_t *found, st_error *err);
int st_graph_reachable(const st_csr *g, uint32_t from, uint32_t to); /* 1, 0, or -1 on bad input */
/* Enumerates root-to-leaf paths from root (lexicographic by label). Paths are written as
 * node index sequences separated by UINT32_MAX into `out` (capacity out_cap); fails with
 * E_BOUND past max_paths paths or out_cap entries. */
st_code st_graph_unfold_paths(const st_csr *g, const char *const *labels, uint32_t root, size_t max_paths,
                              uint32_t *out, size_t out_cap, size_t *npaths, size_t *nwritten, st_error *err);
/* Relabel/shuffle-invariant digest: SHA-256 of the sorted list "from\x1fto\x1ftype\n" plus
 * sorted isolated labels. Duplicate labels are rejected (E_DUPLICATE). */
st_code st_graph_canonical_digest(const char *const *labels, size_t n, const st_edge *edges, size_t m, char hex[65], st_error *err);
/* Provenance deduplication: sorts digests and removes exact repeats in place; returns new count. */
size_t st_dedup_digests(char (*digests)[65], size_t n);

/* ---------- provenance records ---------- */
typedef struct {
  const char *agency, *canonical_id, *release_epoch, *mime, *evidence_sha256;
  const char *source_id, *parent_id;          /* source hierarchy: registry source and containment parent */
  const char *sensor_modality, *recording_origin_id;
  st_tri calibrated, range_telemetry, resolved, corroborated;
  double timestamp_uncertainty_ms;           /* NaN = unknown, never zero */
} st_record;

typedef struct {
  size_t n; st_record *rec; st_json *backing; /* strings point into backing */
  st_csr contains;                              /* forest built from parent_id */
} st_record_set;

/* Loads {"records":[...]} in the fixtures/archives format; validates required strings,
 * tri-state flags (true/false/null/absent=unknown), MIME syntax, unique canonical_id, dangling
 * parents (E_DANGLING), and containment cycles (E_CYCLE). */
st_code st_records_load(st_record_set *rs, const st_json *doc, st_error *err);
void st_records_free(st_record_set *rs);

#endif
