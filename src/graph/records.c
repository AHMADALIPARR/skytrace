#include "skytrace/graph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *const SOURCES[] = {"pursue","aaro","nara","nasa","geipan","nuforc","opensky","celestrak","jpl_fireball","ams",NULL};

static int blank(const st_json *v) {
  if (!v || v->type != ST_JSTR) return 1;
  for (size_t i = 0; i < v->len; ++i) if (v->str[i] != ' ' && v->str[i] != '\t' && v->str[i] != '\n' && v->str[i] != '\r') return 0;
  return 1;
}
static st_code req_str(const st_json *o, const char *k, const char **out, long i, st_error *err) {
  const st_json *v = st_json_get(o, k);
  if (blank(v)) return st_fail(err, ST_E_SCHEMA, i, "required_string:%s", k);
  *out = v->str; return ST_E_OK;
}
static st_code opt_str(const st_json *o, const char *k, const char **out, long i, st_error *err) {
  const st_json *v = st_json_get(o, k); *out = NULL;
  if (!v || v->type == ST_JNULL) return ST_E_OK;
  if (blank(v)) return st_fail(err, ST_E_SCHEMA, i, "invalid_string:%s", k);
  *out = v->str; return ST_E_OK;
}
static st_code tri(const st_json *o, const char *k, st_tri *out, long i, st_error *err) {
  const st_json *v = st_json_get(o, k);
  if (!v || v->type == ST_JNULL) { *out = ST_UNKNOWN; return ST_E_OK; }  /* absent = unknown, never false */
  if (v->type != ST_JBOOL) return st_fail(err, ST_E_SCHEMA, i, "invalid_bool:%s", k);
  *out = v->boolean ? ST_KNOWN_TRUE : ST_KNOWN_FALSE; return ST_E_OK;
}

void st_records_free(st_record_set *rs) { free(rs->rec); st_json_free(rs->backing); st_csr_free(&rs->contains); memset(rs, 0, sizeof *rs); }

st_code st_records_load(st_record_set *rs, const st_json *doc, st_error *err) {
  memset(rs, 0, sizeof *rs);
  const st_json *arr = st_json_get(doc, "records");
  if (!arr || arr->type != ST_JARR) return st_fail(err, ST_E_SCHEMA, -1, "records_not_array");
  if (arr->count > ST_GRAPH_MAX_NODES) return st_fail(err, ST_E_SIZE_LIMIT, -1, "too many records");
  rs->backing = st_json_clone(arr); rs->n = arr->count;
  rs->rec = calloc(rs->n ? rs->n : 1, sizeof *rs->rec);
  if (!rs->backing || !rs->rec) { st_records_free(rs); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  st_code rc = ST_E_OK;
  for (size_t i = 0; i < rs->n && !rc; ++i) {
    const st_json *o = rs->backing->items[i]; st_record *r = &rs->rec[i]; long li = (long)i;
    if (o->type != ST_JOBJ) { rc = st_fail(err, ST_E_SCHEMA, li, "record_not_object"); break; }
    if ((rc = req_str(o, "agency", &r->agency, li, err)) || (rc = req_str(o, "canonical_id", &r->canonical_id, li, err)) ||
        (rc = req_str(o, "release_epoch", &r->release_epoch, li, err)) || (rc = req_str(o, "mime", &r->mime, li, err)) ||
        (rc = req_str(o, "source", &r->source_id, li, err)) || (rc = opt_str(o, "parent_id", &r->parent_id, li, err)) ||
        (rc = opt_str(o, "evidence_sha256", &r->evidence_sha256, li, err)) || (rc = opt_str(o, "sensor_modality", &r->sensor_modality, li, err)) ||
        (rc = opt_str(o, "recording_origin_id", &r->recording_origin_id, li, err)) ||
        (rc = tri(o, "has_range_telemetry", &r->range_telemetry, li, err)) || (rc = tri(o, "corroborated", &r->corroborated, li, err)) ||
        (rc = tri(o, "resolved", &r->resolved, li, err)) || (rc = tri(o, "calibrated", &r->calibrated, li, err))) break;
    if (!strchr(r->mime, '/') || strchr(r->mime, '\n') || strchr(r->mime, '\r')) { rc = st_fail(err, ST_E_SCHEMA, li, "invalid_mime"); break; }
    int known = 0; for (int s = 0; SOURCES[s]; ++s) if (!strcmp(SOURCES[s], r->source_id)) known = 1;
    if (!known) { rc = st_fail(err, ST_E_SOURCE_POLICY, li, "invalid_source"); break; }
    if (r->evidence_sha256 && !st_is_lower_hex(r->evidence_sha256, 64)) { rc = st_fail(err, ST_E_DIGEST, li, "invalid_evidence_sha256"); break; }
    const st_json *u = st_json_get(o, "timestamp_uncertainty_ms");
    if (!u || u->type == ST_JNULL) r->timestamp_uncertainty_ms = NAN;
    else if (u->type != ST_JNUM || u->num < 0) { rc = st_fail(err, ST_E_TIME, li, "invalid_timestamp_uncertainty"); break; }
    else r->timestamp_uncertainty_ms = u->num;
    for (size_t j = 0; j < i; ++j) if (!strcmp(rs->rec[j].canonical_id, r->canonical_id)) { rc = st_fail(err, ST_E_DUPLICATE, li, "ambiguous_parent_id"); break; }
  }
  if (rc) { st_records_free(rs); return rc; }
  st_edge *e = malloc((rs->n ? rs->n : 1) * sizeof *e); size_t m = 0;
  if (!e) { st_records_free(rs); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  for (size_t i = 0; i < rs->n && !rc; ++i) {
    if (!rs->rec[i].parent_id) continue;
    size_t p = rs->n;
    for (size_t j = 0; j < rs->n; ++j) if (!strcmp(rs->rec[j].canonical_id, rs->rec[i].parent_id)) { p = j; break; }
    if (p == rs->n) { rc = st_fail(err, ST_E_DANGLING, (long)i, "unknown_parent_id"); break; }
    if (p == i) { rc = st_fail(err, ST_E_CYCLE, (long)i, "containment_cycle"); break; }
    e[m].from = (uint32_t)p; e[m].to = (uint32_t)i; e[m].type = ST_EDGE_CONTAINS; m++;
  }
  if (!rc) rc = st_csr_build(&rs->contains, rs->n, e, m, err);
  if (!rc) rc = st_graph_containment_check(&rs->contains, err);
  free(e);
  if (rc) st_records_free(rs);
  return rc;
}
