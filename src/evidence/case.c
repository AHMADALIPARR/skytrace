// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/evidence.h"
#include <stdio.h>
#include <string.h>

void st_stable_case_id(const char *sid, size_t sn, const char *rid, size_t rn, char out[20]) {
  st_sha256_ctx c; uint8_t d[32]; char hex[65]; const uint8_t zero = 0;
  st_sha256_init(&c); st_sha256_update(&c, sid, sn); st_sha256_update(&c, &zero, 1); st_sha256_update(&c, rid, rn);
  st_sha256_final(&c, d); st_hex_encode(d, 32, hex);
  memcpy(out, "ST-", 3);
  for (int i = 0; i < 16; ++i) out[3 + i] = (char)(hex[i] >= 'a' ? hex[i] - 32 : hex[i]);
  out[19] = '\0';
}

/* ECMAScript template-literal string conversion for the scalar JSON types. */
static int js_string(const st_json *v, st_buf *b) {
  char num[40];
  switch (v->type) {
    case ST_JSTR: st_buf_put(b, v->str, v->len); return 1;
    case ST_JNULL: st_buf_puts(b, "null"); return 1;
    case ST_JBOOL: st_buf_puts(b, v->boolean ? "true" : "false"); return 1;
    case ST_JNUM: st_format_double_js(v->num, num); st_buf_puts(b, num); return 1;
    default: return 0; /* arrays/objects: rejected (stricter than JS coercion) */
  }
}

static int days_in_month(int y, int m) {
  static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return d[m - 1];
}
static int digits(const char *s, size_t n, size_t at, size_t count, int *out) {
  if (at + count > n) return 0;
  int v = 0;
  for (size_t i = 0; i < count; ++i) { char c = s[at + i]; if (c < '0' || c > '9') return 0; v = v * 10 + (c - '0'); }
  *out = v; return 1;
}
int st_parse_iso8601(const char *s, size_t n) {
  size_t i = 0; int y, mo = 1, d = 1, h = 0, mi = 0, se = 0;
  if (n > 0 && (s[0] == '+' || s[0] == '-')) { if (!digits(s, n, 1, 6, &y)) return 0; if (s[0] == '-' && y == 0) return 0; i = 7; }
  else { if (!digits(s, n, 0, 4, &y)) return 0; i = 4; }
  if (i < n && s[i] == '-') { if (!digits(s, n, i + 1, 2, &mo)) return 0; i += 3;
    if (i < n && s[i] == '-') { if (!digits(s, n, i + 1, 2, &d)) return 0; i += 3; } }
  if (mo < 1 || mo > 12 || d < 1 || d > days_in_month(y, mo)) return 0;
  if (i == n) return 1;
  if (s[i] != 'T' && s[i] != 't') return 0;
  if (!digits(s, n, i + 1, 2, &h) || i + 3 >= n || s[i + 3] != ':' || !digits(s, n, i + 4, 2, &mi)) return 0;
  i += 6;
  int frac = 0;
  if (i < n && s[i] == ':') {
    if (!digits(s, n, i + 1, 2, &se)) { return 0; }
    i += 3;
    if (i < n && s[i] == '.') { size_t j = i + 1; while (j < n && s[j] >= '0' && s[j] <= '9') { if (s[j] != '0') frac = 1; j++; } if (j == i + 1) return 0; i = j; }
  }
  if (h > 24 || mi > 59 || se > 59 || (h == 24 && (mi || se || frac))) return 0;
  if (i == n) return 1;
  if (s[i] == 'Z' || s[i] == 'z') return i + 1 == n;
  if (s[i] == '+' || s[i] == '-') {
    int oh, om;
    if (!digits(s, n, i + 1, 2, &oh) || i + 3 >= n || s[i + 3] != ':' || !digits(s, n, i + 4, 2, &om)) return 0;
    return i + 6 == n && oh <= 23 && om <= 59;
  }
  return 0;
}

static int str_eq(const st_json *v, const char *s) { return v && v->type == ST_JSTR && v->len == strlen(s) && memcmp(v->str, s, v->len) == 0; }
static int in_set(const st_json *v, const char *const *set) { for (; *set; ++set) if (str_eq(v, *set)) return 1; return 0; }

/* data/raw/<64 hex>.<[a-z0-9]+> anchored at both ends. */
static int good_evidence_path(const st_json *p) {
  if (!p || p->type != ST_JSTR || p->len < 9 + 64 + 2 || memcmp(p->str, "data/raw/", 9) != 0) return 0;
  for (size_t i = 9; i < 73; ++i) { char c = p->str[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0; }
  if (p->str[73] != '.') return 0;
  for (size_t i = 74; i < p->len; ++i) { char c = p->str[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))) return 0; }
  return 1;
}

st_code st_validate_case(const st_json *c, st_error *err) {
  static const char *const keys[] = {"case_id","source_id","source_record_id","observed_at","time_precision","classification","evidence","claims",NULL};
  static const char *const prec[] = {"exact","minute","hour","day","month","year","unknown",NULL};
  static const char *const cls[] = {"unassessed","identified","insufficient_data","unresolved_after_analysis",NULL};
  if (!c || (c->type != ST_JOBJ && c->type != ST_JARR)) return st_fail(err, ST_E_SCHEMA, -1, "Case must be an object");
  for (int i = 0; keys[i]; ++i) if (!st_json_has(c, keys[i])) return st_fail(err, ST_E_SCHEMA, -1, "Missing %s", keys[i]);
  st_buf a, b; st_buf_init(&a); st_buf_init(&b);
  int ok = js_string(st_json_get(c, "source_id"), &a) && js_string(st_json_get(c, "source_record_id"), &b);
  char id[20];
  if (ok) st_stable_case_id(a.data ? a.data : "", a.len, b.data ? b.data : "", b.len, id);
  st_buf_free(&a); st_buf_free(&b);
  if (!ok || !str_eq(st_json_get(c, "case_id"), id)) return st_fail(err, ST_E_SCHEMA, -1, "Case ID mismatch");
  const st_json *obs = st_json_get(c, "observed_at");
  if (!(obs->type == ST_JNULL || (obs->type == ST_JSTR && st_parse_iso8601(obs->str, obs->len))))
    return st_fail(err, ST_E_TIME, -1, "Bad observed_at");
  if (!in_set(st_json_get(c, "time_precision"), prec)) return st_fail(err, ST_E_SCHEMA, -1, "Bad precision");
  if (!in_set(st_json_get(c, "classification"), cls)) return st_fail(err, ST_E_SCHEMA, -1, "Bad classification");
  const st_json *ev = st_json_get(c, "evidence"), *cl = st_json_get(c, "claims");
  if (ev->type != ST_JARR || cl->type != ST_JARR) return st_fail(err, ST_E_SCHEMA, -1, "Evidence/claims must be arrays");
  for (size_t i = 0; i < ev->count; ++i) {
    const st_json *e = ev->items[i];
    const st_json *h = e->type == ST_JOBJ ? st_json_get(e, "sha256") : NULL;
    if (!h || h->type != ST_JSTR || h->len != 64 || !st_is_lower_hex(h->str, 64)) return st_fail(err, ST_E_DIGEST, (long)i, "Bad evidence hash");
    if (!good_evidence_path(st_json_get(e, "relative_path"))) return st_fail(err, ST_E_SCHEMA, (long)i, "Bad evidence path");
  }
  return ST_E_OK;
}
