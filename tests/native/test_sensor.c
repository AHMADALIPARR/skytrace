#include "skytrace/sensor.h"
#include "check.h"

static st_json *fixture;
static st_json *fresh(void) { return st_json_clone(fixture); }
static const char *reason(st_json *payload) {
  static char r[64]; st_json *o = st_sensor_graph_analyze(payload);
  const st_json *s = st_json_get(o, "status"), *x = st_json_get(o, "reason");
  snprintf(r, sizeof r, "%s", strcmp(s->str, "ok") == 0 ? "ok" : x->str);
  st_json_free(o); st_json_free(payload); return r;
}
static char *canon(const st_json *v) { st_buf b; st_buf_init(&b); st_json_write(&b, v, ST_JSON_PY_CANONICAL); return b.data; }
static void reverse(st_json *a) { for (size_t i = 0; i < a->count / 2; ++i) { st_json *t = a->items[i]; a->items[i] = a->items[a->count-1-i]; a->items[a->count-1-i] = t; } }
static st_json *obs(st_json *p, size_t i) { return st_json_get(p, "observations")->items[i]; }
static st_json *sample(st_json *p, size_t o, size_t i) { return st_json_get(obs(p, o), "samples")->items[i]; }
static const char *multiset(const st_json *res) { return st_json_get(st_json_get(res, "sensor"), "sensor_multiset_sha256")->str; }
static void del_key(st_json *o, const char *k) {
  for (size_t i = 0; i < o->count; ++i) if (!strcmp(o->keys[i], k)) {
    st_json_free(o->items[i]); free(o->keys[i]);
    memmove(o->items + i, o->items + i + 1, (o->count - i - 1) * sizeof *o->items);
    memmove(o->keys + i, o->keys + i + 1, (o->count - i - 1) * sizeof *o->keys);
    memmove(o->klens + i, o->klens + i + 1, (o->count - i - 1) * sizeof *o->klens);
    o->count--; return;
  }
}
static st_json *edge(const char *f, const char *t, const char *ty) {
  st_json *e = st_json_object(); st_json_set(e, "from", st_json_string(f)); st_json_set(e, "to", st_json_string(t)); st_json_set(e, "type", st_json_string(ty)); return e;
}

int main(int argc, char **argv) {
  const char *src = argc > 1 ? argv[1] : ".";
  char path[4200]; st_error e;
  snprintf(path, sizeof path, "%s/fixtures/sensors/synthetic-graph.json", src);
  fixture = st_json_parse_file(path, &e); CHECK(fixture != NULL); if (!fixture) return finish("test_sensor");

  /* Byte-identical to the frozen output of numerics/sensor_graph.py on the NDJSON fixture. */
  char *nd, *ref; size_t ndn, refn;
  snprintf(path, sizeof path, "%s/fixtures/sensors/synthetic-graph.ndjson", src); CHECK(st_read_file(path, &nd, &ndn, 1 << 20, &e) == ST_E_OK);
  snprintf(path, sizeof path, "%s/fixtures/reference/sensor_graph.synthetic-graph.out.json", src); CHECK(st_read_file(path, &ref, &refn, 1 << 20, &e) == ST_E_OK);
  st_buf out; st_buf_init(&out); st_sensor_graph_stream(nd, ndn, &out);
  CHECK(out.len == refn && memcmp(out.data, ref, refn) == 0);
  if (out.len != refn || memcmp(out.data, ref, refn)) fprintf(stderr, "got:\n%s\nwant:\n%s\n", out.data, ref);
  st_buf_free(&out); free(nd); free(ref);

  /* Ports of numerics/tests/test_sensor_graph.py */
  st_json *r = st_sensor_graph_analyze(fixture);
  st_json *sensor = st_json_get(r, "sensor"), *graph = st_json_get(r, "graph");
  CHECK(st_json_get(sensor, "observation_count")->ival == 2);
  CHECK(st_json_get(sensor, "independent_recording_origins")->ival == 1);
  CHECK(st_json_get(sensor, "missing_range_count")->ival == 2);
  CHECK(st_json_get(st_json_get(graph, "root_reachable_observations"), "archive")->ival == 2);  /* join deduplicated */
  st_json *tracks = st_json_get(r, "tracks");
  CHECK_NEAR(st_json_get(st_json_get(tracks->items[0], "angular_rates")->items[0], "angular_norm_rad_s")->num, 0.02, 1e-7);
  for (size_t i = 0; i < tracks->count; ++i) { const st_json *m = st_json_get(tracks->items[i], "metric_speed_identifiable"); CHECK(m->type == ST_JBOOL && !m->boolean); }
  char *base = canon(r);
  st_json *sh = fresh(); reverse(st_json_get(sh, "nodes")); reverse(st_json_get(sh, "edges")); reverse(st_json_get(sh, "observations"));
  st_json *r2 = st_sensor_graph_analyze(sh); char *shuf = canon(r2); CHECK_STR(shuf, base); free(shuf); st_json_free(r2); st_json_free(sh);
  st_json *forged = fresh(); st_json_set(obs(forged, 1), "recording_origin_id", st_json_string("fabricated-second-camera"));
  r2 = st_sensor_graph_analyze(forged);
  CHECK(strcmp(multiset(r2), multiset(r)) != 0);
  CHECK(st_json_get(st_json_get(r2, "sensor"), "independent_recording_origins")->ival == 2);
  st_json_free(r2); st_json_free(forged);
  st_json *drop = fresh(); st_json *smp = st_json_get(obs(drop, 0), "samples"); st_json_free(smp->items[--smp->count]);
  r2 = st_sensor_graph_analyze(drop); CHECK(strcmp(multiset(r2), multiset(r)) != 0); st_json_free(r2); st_json_free(drop);
  st_json *cy = fresh(); st_json_push(st_json_get(cy, "edges"), edge("record-b", "case", "derived_from")); CHECK_STR(reason(cy), "E_CYCLE");
  st_json *dc = fresh(); st_json_push(st_json_get(dc, "edges"), edge("archive", "record-a", "contains")); CHECK_STR(reason(dc), "E_MULTIPLE_CONTAINMENT_PARENTS");
  st_json *tr = fresh(); st_json_set(sample(tr, 0, 1), "t_s", st_json_int(-1)); CHECK_STR(reason(tr), "E_TIME_ORDER");
  st_json *mc = fresh(); del_key(obs(mc, 0), "calibration_sha256"); CHECK_STR(reason(mc), "E_CALIBRATION_STATE");
  st_json *az = fresh(); st_json_set(sample(az, 0, 1), "az_rad", st_json_int(100)); CHECK_STR(reason(az), "E_AZIMUTH");
  st_json *zr = fresh(); st_json_set(obs(zr, 0), "range_m", st_json_int(0)); CHECK_STR(reason(zr), "E_RANGE");
  /* further adversarial cases */
  st_json *se = fresh(); st_json_push(st_json_get(se, "edges"), edge("case", "case", "derived_from")); CHECK_STR(reason(se), "E_SELF_CYCLE");
  st_json *de = fresh(); st_json_push(st_json_get(de, "edges"), edge("archive", "case", "contains")); CHECK_STR(reason(de), "E_DUP_EDGE");
  st_json *dn = fresh(); st_json_push(st_json_get(dn, "edges"), edge("archive", "ghost", "contains")); CHECK_STR(reason(dn), "E_EDGE_REF");
  st_json *bt = fresh(); st_json_push(st_json_get(bt, "edges"), edge("archive", "case", "wormhole")); CHECK_STR(reason(bt), "E_EDGE_REF");
  st_json *dobs = fresh(); st_json_set(obs(dobs, 1), "observation_id", st_json_string("synthetic-1")); CHECK_STR(reason(dobs), "E_DUP_OBSERVATION");
  st_json *on = fresh(); st_json_set(obs(on, 0), "node_id", st_json_string("nowhere")); CHECK_STR(reason(on), "E_OBSERVATION_NODE");
  st_json *el = fresh(); st_json_set(sample(el, 0, 0), "el_rad", st_json_double(2.0)); CHECK_STR(reason(el), "E_ELEVATION");
  st_json *bc = fresh(); st_json_set(obs(bc, 0), "calibration_sha256", st_json_string("XYZ")); CHECK_STR(reason(bc), "E_CALIBRATION_HASH");
  st_json *bo = fresh(); st_json_set(sample(bo, 0, 0), "t_s", st_json_bool(1)); CHECK_STR(reason(bo), "E_NONFINITE"); /* bool is not a number */
  st_json *one = fresh(); st_json *s1 = st_json_get(obs(one, 0), "samples"); while (s1->count > 1) st_json_free(s1->items[--s1->count]); CHECK_STR(reason(one), "E_SAMPLES_MIN_TWO");
  st_json *nr = fresh(); del_key(obs(nr, 0), "range_m"); CHECK_STR(reason(nr), "'range_m'");
  st_json *amb = fresh(); st_json_set(sample(amb, 0, 0), "az_rad", st_json_double(-1.5707963267948966)); st_json_set(sample(amb, 0, 1), "az_rad", st_json_double(1.5707963267948966));
  CHECK_STR(reason(amb), "E_AZ_UNWRAP_AMBIGUOUS");
  CHECK_STR(reason(st_json_array()), "E_PAYLOAD_TYPE");
  /* A measured range is never turned into a metric speed claim by itself. */
  st_json *wr = fresh(); st_json_set(obs(wr, 0), "range_m", st_json_double(1200.0));
  r2 = st_sensor_graph_analyze(wr);
  st_json *t0 = st_json_get(r2, "tracks")->items[0];
  CHECK(st_json_get(t0, "metric_speed_identifiable")->type == ST_JNULL);
  CHECK_STR(st_json_get(t0, "identifiability_note")->str, "range alone is insufficient without sensor-platform motion and calibration");
  st_json_free(r2); st_json_free(wr);
  /* Angle wrap uses Python modulo semantics across the +/-pi seam. */
  CHECK_NEAR(st_wrap_angle(3.1 - (-3.1)), 6.2 - 2 * 3.14159265358979323846, 1e-15);
  CHECK(st_py_fmod(-1.0, 3.0) == 2.0 && st_py_fmod(1.0, -3.0) == -2.0);
  free(base); st_json_free(r); st_json_free(fixture);
  return finish("test_sensor");
}
