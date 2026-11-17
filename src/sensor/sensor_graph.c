#include "skytrace/sensor.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.14159265358979323846;

double st_py_fmod(double a, double b) {
  double m = fmod(a, b);
  if (m != 0) { if ((b < 0) != (m < 0)) m += b; }
  else m = copysign(0.0, b);
  return m;
}
double st_wrap_angle(double d) { return st_py_fmod(d + PI, 2 * PI) - PI; }

st_code st_bearing_rates(const st_bearing_sample *s, size_t n, st_angular_rate *out, const char **label, st_error *err) {
#define FAIL(L) do { *label = L; return st_fail(err, ST_E_SCHEMA, -1, "%s", L); } while (0)
  if (n < 2) FAIL("E_SAMPLES_MIN_TWO");
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(s[i].t_s) || !isfinite(s[i].az_rad) || !isfinite(s[i].el_rad)) FAIL("E_NONFINITE");
    if (!(-PI / 2 <= s[i].el_rad && s[i].el_rad <= PI / 2)) FAIL("E_ELEVATION");
    if (!(-PI <= s[i].az_rad && s[i].az_rad <= PI)) FAIL("E_AZIMUTH");
  }
  for (size_t i = 1; i < n; ++i) if (!(s[i].t_s > s[i-1].t_s)) FAIL("E_TIME_ORDER");
  for (size_t i = 1; i < n; ++i) {
    double da = st_wrap_angle(s[i].az_rad - s[i-1].az_rad);
    if (!(fabs(fabs(da) - PI) > 1e-10)) FAIL("E_AZ_UNWRAP_AMBIGUOUS");
    double de = s[i].el_rad - s[i-1].el_rad;
    st_angular_rate *r = &out[i-1];
    r->dt_is_int = s[i].t_is_int && s[i-1].t_is_int;
    if (r->dt_is_int) { r->dt_int = s[i].t_int - s[i-1].t_int; r->dt_s = (double)r->dt_int; }
    else r->dt_s = s[i].t_s - s[i-1].t_s;
    r->az_rad_s = da / r->dt_s; r->el_rad_s = de / r->dt_s;
    r->angular_norm_rad_s = hypot(r->az_rad_s, r->el_rad_s);
  }
  return ST_E_OK;
#undef FAIL
}

/* ---------- graph/sensor analysis (numerics/sensor_graph.py contract) ---------- */
typedef struct { const char *label; } fail_t;

static st_json *error_obj(const char *reason) {
  st_json *o = st_json_object();
  st_json_set(o, "reason", st_json_string(reason)); st_json_set(o, "status", st_json_string("error"));
  return o;
}
static int finite_num(const st_json *v) { return v && v->type == ST_JNUM && isfinite(v->num); }
static int nonempty_str(const st_json *v) { return v && v->type == ST_JSTR && v->len > 0; }
static int find_id(const st_json *ids, const st_json *v) {
  if (!v || v->type != ST_JSTR) return -1;
  for (size_t i = 0; i < ids->count; ++i) if (ids->items[i]->len == v->len && !memcmp(ids->items[i]->str, v->str, v->len)) return (int)i;
  return -1;
}
static char *canon_digest(const st_json *v, char hex[65]) {
  st_buf b; st_buf_init(&b); st_json_write(&b, v, ST_JSON_PY_CANONICAL);
  st_sha256_hex(b.data, b.len, hex); st_buf_free(&b); return hex;
}
static int cmp_hexp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static int cmp_track(const void *a, const void *b) {
  const st_json *x = *(st_json *const *)a, *y = *(st_json *const *)b;
  const st_json *ix = st_json_get(x, "observation_id"), *iy = st_json_get(y, "observation_id");
  size_t m = ix->len < iy->len ? ix->len : iy->len; int c = memcmp(ix->str, iy->str, m);
  return c ? c : (ix->len > iy->len) - (ix->len < iy->len);
}

/* Returns a signature object or NULL with *reason set. */
static st_json *signature(const st_json *o, const char **reason) {
  static const char *const fields[] = {"observation_id","recording_origin_id","modality","calibration_sha256","range_m","samples",NULL};
  if (!nonempty_str(st_json_get(o, "observation_id"))) { *reason = "E_OBSERVATION_ID"; return NULL; }
  if (!nonempty_str(st_json_get(o, "recording_origin_id"))) { *reason = "E_ORIGIN"; return NULL; }
  if (!nonempty_str(st_json_get(o, "modality"))) { *reason = "E_MODALITY"; return NULL; }
  const st_json *cal = st_json_get(o, "calibration_sha256");
  if (!cal) { *reason = "E_CALIBRATION_STATE"; return NULL; }
  if (!(cal->type == ST_JNULL || (cal->type == ST_JSTR && cal->len == 64 && st_is_lower_hex(cal->str, 64)))) { *reason = "E_CALIBRATION_HASH"; return NULL; }
  const st_json *rg = st_json_get(o, "range_m");
  if (!(!rg || rg->type == ST_JNULL || (finite_num(rg) && rg->num > 0))) { *reason = "E_RANGE"; return NULL; }
  const st_json *sm = st_json_get(o, "samples");
  size_t n = sm && (sm->type == ST_JARR) ? sm->count : 0;
  if (sm && sm->type == ST_JOBJ) n = sm->count;
  if (n < 2) { *reason = "E_SAMPLES_MIN_TWO"; return NULL; }
  if (sm->type != ST_JARR) { *reason = "E_SAMPLES_TYPE"; return NULL; }
  st_bearing_sample *bs = calloc(n, sizeof *bs); st_angular_rate *rt = calloc(n, sizeof *rt);
  for (size_t i = 0; i < n; ++i) {
    const st_json *s = sm->items[i];
    if (s->type != ST_JOBJ) { free(bs); free(rt); *reason = "E_SAMPLE_TYPE"; return NULL; }
    const st_json *t = st_json_get(s, "t_s"), *a = st_json_get(s, "az_rad"), *e = st_json_get(s, "el_rad");
    if (!finite_num(t) || !finite_num(a) || !finite_num(e)) { free(bs); free(rt); *reason = "E_NONFINITE"; return NULL; }
    bs[i].t_s = t->num; bs[i].t_is_int = t->is_int; bs[i].t_int = t->ival; bs[i].az_rad = a->num; bs[i].el_rad = e->num;
  }
  st_error err; const char *label = NULL;
  if (st_bearing_rates(bs, n, rt, &label, &err)) { free(bs); free(rt); *reason = label; return NULL; }
  if (!rg) { free(bs); free(rt); *reason = "'range_m'"; return NULL; }  /* Python KeyError text */
  st_json *exact = st_json_object();
  for (int k = 0; fields[k]; ++k) st_json_set(exact, fields[k], st_json_clone(st_json_get(o, fields[k])));
  char hex[65]; canon_digest(exact, hex); st_json_free(exact);
  st_json *rates = st_json_array();
  for (size_t i = 0; i + 1 < n; ++i) {
    st_json *r = st_json_object();
    st_json_set(r, "dt_s", rt[i].dt_is_int ? st_json_int(rt[i].dt_int) : st_json_double(rt[i].dt_s));
    st_json_set(r, "az_rad_s", st_json_double(rt[i].az_rad_s));
    st_json_set(r, "el_rad_s", st_json_double(rt[i].el_rad_s));
    st_json_set(r, "angular_norm_rad_s", st_json_double(rt[i].angular_norm_rad_s));
    st_json_push(rates, r);
  }
  free(bs); free(rt);
  int no_range = rg->type == ST_JNULL;
  st_json *sig = st_json_object();
  st_json_set(sig, "observation_id", st_json_clone(st_json_get(o, "observation_id")));
  st_json_set(sig, "recording_origin_id", st_json_clone(st_json_get(o, "recording_origin_id")));
  st_json_set(sig, "semantic_sha256", st_json_string(hex));
  st_json_set(sig, "angular_rates", rates);
  st_json_set(sig, "metric_speed_identifiable", no_range ? st_json_bool(0) : st_json_null());
  st_json_set(sig, "identifiability_note", st_json_string(no_range ? "unknown range blocks metric speed"
                 : "range alone is insufficient without sensor-platform motion and calibration"));
  return sig;
}

st_json *st_sensor_graph_analyze(const st_json *p) {
  if (!p || p->type != ST_JOBJ) return error_obj("E_PAYLOAD_TYPE");
  const st_json *nodes = st_json_get(p, "nodes"), *edges = st_json_get(p, "edges"), *obs = st_json_get(p, "observations");
  if (!nodes || nodes->type != ST_JARR || !edges || edges->type != ST_JARR || !obs || obs->type != ST_JARR) return error_obj("E_INPUT_ARRAYS");
  if (nodes->count > 20000 || edges->count > 100000 || obs->count > 20000) return error_obj("E_SIZE_LIMIT");
  st_json *ids = st_json_array();
  for (size_t i = 0; i < nodes->count; ++i) {
    const st_json *id = nodes->items[i]->type == ST_JOBJ ? st_json_get(nodes->items[i], "id") : NULL;
    if (!nonempty_str(id) || find_id(ids, id) >= 0) { st_json_free(ids); return error_obj("E_NODE_IDS"); }
    st_json_push(ids, st_json_clone(id));
  }
  size_t n = ids->count, m = edges->count;
  st_edge *e = malloc((m ? m : 1) * sizeof *e);
  const char *reason = NULL;
  for (size_t i = 0; i < m && !reason; ++i) {
    const st_json *x = edges->items[i];
    const st_json *f = st_json_get(x, "from"), *t = st_json_get(x, "to"), *ty = st_json_get(x, "type");
    int a = find_id(ids, f), b = find_id(ids, t); st_edge_type et;
    if (a < 0 || b < 0 || !ty || ty->type != ST_JSTR || !st_edge_type_parse(ty->str, ty->len, &et)) { reason = "E_EDGE_REF"; break; }
    if (a == b) { reason = "E_SELF_CYCLE"; break; }
    for (size_t j = 0; j < i; ++j) if (e[j].from == (uint32_t)a && e[j].to == (uint32_t)b && e[j].type == et) { reason = "E_DUP_EDGE"; break; }
    e[i].from = (uint32_t)a; e[i].to = (uint32_t)b; e[i].type = et;
  }
  st_csr g; st_error err; memset(&g, 0, sizeof g);
  const char **labels = malloc((n ? n : 1) * sizeof *labels);
  for (size_t i = 0; i < n; ++i) labels[i] = ids->items[i]->str;
  uint32_t *order = malloc((n ? n : 1) * sizeof *order);
  if (!reason && st_csr_build(&g, n, e, m, &err)) reason = "E_EDGE_REF";
  if (!reason && st_graph_toposort(&g, labels, order, &err)) reason = "E_CYCLE";
  if (!reason && st_graph_containment_check(&g, &err)) reason = err.code == ST_E_CYCLE ? "E_CYCLE" : "E_MULTIPLE_CONTAINMENT_PARENTS";
  st_json *sigs = st_json_array(); st_json *seen = st_json_array();
  int *obs_node = calloc(obs->count ? obs->count : 1, sizeof *obs_node);
  size_t calibrated = 0, missing = 0;
  for (size_t i = 0; i < obs->count && !reason; ++i) {
    const st_json *o = obs->items[i];
    if (o->type != ST_JOBJ) { reason = "E_OBSERVATION_TYPE"; break; }
    const st_json *oid = st_json_get(o, "observation_id");
    if (oid && oid->type == ST_JSTR && find_id(seen, oid) >= 0) { reason = "E_DUP_OBSERVATION"; break; }
    if (oid && oid->type == ST_JSTR) st_json_push(seen, st_json_clone(oid));
    int node = find_id(ids, st_json_get(o, "node_id"));
    if (node < 0) { reason = "E_OBSERVATION_NODE"; break; }
    st_json *sig = signature(o, &reason);
    if (!sig) break;
    st_json_set(sig, "node_id", st_json_clone(st_json_get(o, "node_id")));
    obs_node[i] = node;
    const st_json *cal = st_json_get(o, "calibration_sha256");
    if (cal->type != ST_JNULL) calibrated++;
    if (st_json_get(sig, "metric_speed_identifiable")->type == ST_JBOOL) missing++;
    st_json_push(sigs, sig);
  }
  st_json *result = NULL;
  if (reason) result = error_obj(reason);
  else {
    /* independent recording origins */
    st_json *origins = st_json_array();
    for (size_t i = 0; i < sigs->count; ++i) { const st_json *r = st_json_get(sigs->items[i], "recording_origin_id"); if (find_id(origins, r) < 0) st_json_push(origins, st_json_clone(r)); }
    char (*hs)[65] = malloc((sigs->count ? sigs->count : 1) * sizeof *hs);
    for (size_t i = 0; i < sigs->count; ++i) memcpy(hs[i], st_json_get(sigs->items[i], "semantic_sha256")->str, 65);
    qsort(hs, sigs->count, sizeof *hs, cmp_hexp);
    st_json *harr = st_json_array(); for (size_t i = 0; i < sigs->count; ++i) st_json_push(harr, st_json_string(hs[i]));
    char multiset[65]; canon_digest(harr, multiset); st_json_free(harr); free(hs);
    st_json *sensor = st_json_object();
    st_json_set(sensor, "sensor_multiset_sha256", st_json_string(multiset));
    st_json_set(sensor, "observation_count", st_json_int((int64_t)sigs->count));
    st_json_set(sensor, "independent_recording_origins", st_json_int((int64_t)origins->count));
    st_json_set(sensor, "missing_range_count", st_json_int((int64_t)missing));
    st_json_set(sensor, "calibrated_count", st_json_int((int64_t)calibrated));
    st_json_free(origins);
    /* roots (no parents of any type), sorted; reachable distinct observations (joins deduplicated) */
    st_json *roots = st_json_array(), *reach = st_json_object();
    size_t *ri = malloc((n ? n : 1) * sizeof *ri); size_t nr = 0;
    for (size_t v = 0; v < n; ++v) if (g.in_row[v] == g.in_row[v+1]) ri[nr++] = v;
    for (size_t i = 1; i < nr; ++i) { size_t x = ri[i], j = i; while (j && strcmp(labels[x], labels[ri[j-1]]) < 0) { ri[j] = ri[j-1]; j--; } ri[j] = x; }
    uint8_t *mask = malloc(n ? n : 1);
    for (size_t k = 0; k < nr; ++k) {
      size_t found; st_graph_reach(&g, (uint32_t)ri[k], 0, mask, n, &found, &err);
      mask[ri[k]] = 1; size_t cnt = 0;
      for (size_t i = 0; i < sigs->count; ++i) if (mask[obs_node[i]]) cnt++;
      st_json_push(roots, st_json_string(labels[ri[k]]));
      st_json_set(reach, labels[ri[k]], st_json_int((int64_t)cnt));
    }
    free(mask); free(ri);
    st_json *graph = st_json_object();
    st_json_set(graph, "nodes", st_json_int((int64_t)n)); st_json_set(graph, "edges", st_json_int((int64_t)m));
    st_json_set(graph, "roots", roots); st_json_set(graph, "root_reachable_observations", reach);
    st_json_set(graph, "acyclic", st_json_bool(1)); st_json_set(graph, "containment_unique_parent", st_json_bool(1));
    qsort(sigs->items, sigs->count, sizeof *sigs->items, cmp_track);
    result = st_json_object();
    st_json_set(result, "status", st_json_string("ok")); st_json_set(result, "graph", graph);
    st_json_set(result, "sensor", sensor); st_json_set(result, "tracks", sigs); sigs = NULL;
  }
  st_json_free(sigs); st_json_free(seen); st_json_free(ids); free(obs_node);
  free(e); free(labels); free(order); st_csr_free(&g);
  return result;
}

int st_sensor_graph_stream(const char *text, size_t len, st_buf *out) {
  size_t i = 0;
  while (i < len) {
    const char *nl = memchr(text + i, '\n', len - i);
    size_t end = nl ? (size_t)(nl - text) : len;
    st_error err; st_json *req = st_json_parse(text + i, end - i, &err);
    st_json *res = req ? st_sensor_graph_analyze(req) : error_obj("E_JSON");
    st_json_write(out, res, ST_JSON_PY_CANONICAL); st_buf_put(out, "\n", 1);
    st_json_free(req); st_json_free(res);
    i = end + 1;
  }
  return !out->failed;
}
