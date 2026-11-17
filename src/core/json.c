/* Strict RFC 8259 JSON: validated UTF-8, no NaN/Infinity, bounded depth and size.
 * Writers reproduce Python json.dumps(sort_keys, compact, ensure_ascii=False),
 * ECMAScript JSON.stringify(v, null, 2), and RFC 8785 canonical bytes. */
#include "skytrace/core.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static st_json *jnew(st_jtype t) { st_json *v = calloc(1, sizeof *v); if (v) v->type = t; return v; }
st_json *st_json_null(void) { return jnew(ST_JNULL); }
st_json *st_json_bool(int b) { st_json *v = jnew(ST_JBOOL); if (v) v->boolean = !!b; return v; }
st_json *st_json_int(int64_t i) { st_json *v = jnew(ST_JNUM); if (v) { v->is_int = 1; v->ival = i; v->num = (double)i; } return v; }
st_json *st_json_double(double d) { st_json *v = jnew(ST_JNUM); if (v) v->num = d; return v; }
st_json *st_json_stringn(const char *s, size_t n) {
  st_json *v = jnew(ST_JSTR); if (!v) return NULL;
  v->str = malloc(n + 1); if (!v->str) { free(v); return NULL; }
  memcpy(v->str, s, n); v->str[n] = '\0'; v->len = n; return v;
}
st_json *st_json_string(const char *s) { return st_json_stringn(s, strlen(s)); }
st_json *st_json_array(void) { return jnew(ST_JARR); }
st_json *st_json_object(void) { return jnew(ST_JOBJ); }

void st_json_free(st_json *v) {
  if (!v) return;
  for (size_t i = 0; i < v->count; ++i) { st_json_free(v->items[i]); if (v->keys) free(v->keys[i]); }
  free(v->items); free(v->keys); free(v->klens); free(v->str); free(v);
}

static int grow(st_json *c) {
  if (c->count < c->cap) return 1;
  size_t cap = c->cap ? c->cap * 2 : 4;
  st_json **it = realloc(c->items, cap * sizeof *it); if (!it) return 0; c->items = it;
  if (c->type == ST_JOBJ) {
    char **k = realloc(c->keys, cap * sizeof *k); if (!k) return 0; c->keys = k;
    size_t *kl = realloc(c->klens, cap * sizeof *kl); if (!kl) return 0; c->klens = kl;
  }
  c->cap = cap; return 1;
}
int st_json_push(st_json *a, st_json *v) {
  if (!a || !v || a->type != ST_JARR || !grow(a)) { st_json_free(v); return 0; }
  a->items[a->count++] = v; return 1;
}
static int set_n(st_json *o, const char *key, size_t kn, st_json *v) {
  if (!o || !v || o->type != ST_JOBJ) { st_json_free(v); return 0; }
  for (size_t i = 0; i < o->count; ++i)
    if (o->klens[i] == kn && memcmp(o->keys[i], key, kn) == 0) { st_json_free(o->items[i]); o->items[i] = v; return 1; }
  if (!grow(o)) { st_json_free(v); return 0; }
  char *k = malloc(kn + 1); if (!k) { st_json_free(v); return 0; }
  memcpy(k, key, kn); k[kn] = '\0';
  o->keys[o->count] = k; o->klens[o->count] = kn; o->items[o->count++] = v; return 1;
}
int st_json_set(st_json *o, const char *key, st_json *v) { return set_n(o, key, strlen(key), v); }

st_json *st_json_get(const st_json *o, const char *key) {
  if (!o || o->type != ST_JOBJ) return NULL;
  size_t kn = strlen(key);
  for (size_t i = 0; i < o->count; ++i) if (o->klens[i] == kn && memcmp(o->keys[i], key, kn) == 0) return o->items[i];
  return NULL;
}
int st_json_has(const st_json *o, const char *key) { return st_json_get(o, key) != NULL; }

st_json *st_json_clone(const st_json *v) {
  if (!v) return NULL;
  st_json *c = jnew(v->type); if (!c) return NULL;
  c->boolean = v->boolean; c->num = v->num; c->is_int = v->is_int; c->ival = v->ival;
  if (v->type == ST_JSTR) { st_json_free(c); return st_json_stringn(v->str, v->len); }
  for (size_t i = 0; i < v->count; ++i) {
    st_json *x = st_json_clone(v->items[i]);
    int ok = v->type == ST_JARR ? st_json_push(c, x) : set_n(c, v->keys[i], v->klens[i], x);
    if (!ok) { st_json_free(c); return NULL; }
  }
  return c;
}

int st_json_equal(const st_json *a, const st_json *b) {
  if (a->type != b->type) return 0;
  switch (a->type) {
    case ST_JNULL: return 1;
    case ST_JBOOL: return a->boolean == b->boolean;
    case ST_JNUM: return a->is_int && b->is_int ? a->ival == b->ival : a->num == b->num;
    case ST_JSTR: return a->len == b->len && memcmp(a->str, b->str, a->len) == 0;
    case ST_JARR:
      if (a->count != b->count) return 0;
      for (size_t i = 0; i < a->count; ++i) if (!st_json_equal(a->items[i], b->items[i])) return 0;
      return 1;
    case ST_JOBJ:
      if (a->count != b->count) return 0;
      for (size_t i = 0; i < a->count; ++i) {
        const st_json *o = NULL;
        for (size_t j = 0; j < b->count; ++j)
          if (b->klens[j] == a->klens[i] && memcmp(b->keys[j], a->keys[i], a->klens[i]) == 0) { o = b->items[j]; break; }
        if (!o || !st_json_equal(a->items[i], o)) return 0;
      }
      return 1;
  }
  return 0;
}

/* ---------- parser ---------- */
typedef struct { const char *p, *end, *start; st_error *err; int failed; } parser;

static void perr(parser *ps, const char *what) {
  if (!ps->failed) st_fail(ps->err, ST_E_JSON, (long)(ps->p - ps->start), "Invalid JSON: %s at byte %ld", what, (long)(ps->p - ps->start));
  ps->failed = 1;
}
static void ws(parser *ps) { while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r')) ps->p++; }

static int utf8_put(st_buf *b, unsigned cp) {
  unsigned char c[4]; size_t n;
  if (cp < 0x80) { c[0] = (unsigned char)cp; n = 1; }
  else if (cp < 0x800) { c[0] = (unsigned char)(0xC0 | cp >> 6); c[1] = (unsigned char)(0x80 | (cp & 63)); n = 2; }
  else if (cp < 0x10000) { c[0] = (unsigned char)(0xE0 | cp >> 12); c[1] = (unsigned char)(0x80 | (cp >> 6 & 63)); c[2] = (unsigned char)(0x80 | (cp & 63)); n = 3; }
  else { c[0] = (unsigned char)(0xF0 | cp >> 18); c[1] = (unsigned char)(0x80 | (cp >> 12 & 63)); c[2] = (unsigned char)(0x80 | (cp >> 6 & 63)); c[3] = (unsigned char)(0x80 | (cp & 63)); n = 4; }
  st_buf_put(b, c, n); return 1;
}
/* Decodes one UTF-8 scalar at s (bounded by e); returns length or 0 if invalid. */
static size_t utf8_decode(const unsigned char *s, const unsigned char *e, unsigned *cp) {
  if (s >= e) return 0;
  unsigned c = s[0];
  if (c < 0x80) { *cp = c; return 1; }
  size_t n; unsigned min;
  if ((c & 0xE0) == 0xC0) { n = 2; *cp = c & 0x1F; min = 0x80; }
  else if ((c & 0xF0) == 0xE0) { n = 3; *cp = c & 0x0F; min = 0x800; }
  else if ((c & 0xF8) == 0xF0) { n = 4; *cp = c & 0x07; min = 0x10000; }
  else return 0;
  if ((size_t)(e - s) < n) return 0;
  for (size_t i = 1; i < n; ++i) { if ((s[i] & 0xC0) != 0x80) return 0; *cp = *cp << 6 | (s[i] & 0x3F); }
  if (*cp < min || *cp > 0x10FFFF || (*cp >= 0xD800 && *cp <= 0xDFFF)) return 0;
  return n;
}
static int hex4(parser *ps, unsigned *out) {
  if (ps->end - ps->p < 4) return 0;
  unsigned v = 0;
  for (int i = 0; i < 4; ++i) {
    char c = ps->p[i]; v <<= 4;
    if (c >= '0' && c <= '9') v |= (unsigned)(c - '0'); else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10); else return 0;
  }
  ps->p += 4; *out = v; return 1;
}
static int parse_string(parser *ps, st_buf *out) {
  ps->p++; /* opening quote */
  while (1) {
    if (ps->p >= ps->end) { perr(ps, "unterminated string"); return 0; }
    unsigned char c = (unsigned char)*ps->p;
    if (c == '"') { ps->p++; return 1; }
    if (c < 0x20) { perr(ps, "control character in string"); return 0; }
    if (c == '\\') {
      ps->p++;
      if (ps->p >= ps->end) { perr(ps, "bad escape"); return 0; }
      char e = *ps->p++;
      switch (e) {
        case '"': st_buf_put(out, "\"", 1); break; case '\\': st_buf_put(out, "\\", 1); break;
        case '/': st_buf_put(out, "/", 1); break; case 'b': st_buf_put(out, "\b", 1); break;
        case 'f': st_buf_put(out, "\f", 1); break; case 'n': st_buf_put(out, "\n", 1); break;
        case 'r': st_buf_put(out, "\r", 1); break; case 't': st_buf_put(out, "\t", 1); break;
        case 'u': {
          unsigned u;
          if (!hex4(ps, &u)) { perr(ps, "bad \\u escape"); return 0; }
          if (u >= 0xD800 && u <= 0xDBFF) {
            unsigned lo;
            if (ps->end - ps->p < 6 || ps->p[0] != '\\' || ps->p[1] != 'u') { perr(ps, "lone surrogate"); return 0; }
            ps->p += 2;
            if (!hex4(ps, &lo) || lo < 0xDC00 || lo > 0xDFFF) { perr(ps, "lone surrogate"); return 0; }
            u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
          } else if (u >= 0xDC00 && u <= 0xDFFF) { perr(ps, "lone surrogate"); return 0; }
          utf8_put(out, u); break;
        }
        default: perr(ps, "bad escape"); return 0;
      }
      continue;
    }
    unsigned cp; size_t n = utf8_decode((const unsigned char *)ps->p, (const unsigned char *)ps->end, &cp);
    if (!n) { perr(ps, "invalid UTF-8"); return 0; }
    st_buf_put(out, ps->p, n); ps->p += n;
  }
}
static st_json *parse_value(parser *ps, int depth);
static st_json *parse_number(parser *ps) {
  const char *s = ps->p; int integral = 1;
  if (ps->p < ps->end && *ps->p == '-') ps->p++;
  if (ps->p >= ps->end) { perr(ps, "bad number"); return NULL; }
  if (*ps->p == '0') ps->p++;
  else if (*ps->p >= '1' && *ps->p <= '9') while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') ps->p++;
  else { perr(ps, "bad number"); return NULL; }
  if (ps->p < ps->end && *ps->p == '.') {
    integral = 0; ps->p++;
    if (ps->p >= ps->end || *ps->p < '0' || *ps->p > '9') { perr(ps, "bad fraction"); return NULL; }
    while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') ps->p++;
  }
  if (ps->p < ps->end && (*ps->p == 'e' || *ps->p == 'E')) {
    integral = 0; ps->p++;
    if (ps->p < ps->end && (*ps->p == '+' || *ps->p == '-')) ps->p++;
    if (ps->p >= ps->end || *ps->p < '0' || *ps->p > '9') { perr(ps, "bad exponent"); return NULL; }
    while (ps->p < ps->end && *ps->p >= '0' && *ps->p <= '9') ps->p++;
  }
  size_t n = (size_t)(ps->p - s);
  char tmp[512];
  if (n >= sizeof tmp) { perr(ps, "number too long"); return NULL; }
  memcpy(tmp, s, n); tmp[n] = '\0';
  st_json *v = jnew(ST_JNUM); if (!v) { perr(ps, "out of memory"); return NULL; }
  v->num = strtod(tmp, NULL);
  if (!isfinite(v->num)) { st_json_free(v); perr(ps, "number out of double range"); return NULL; }
  if (integral) {
    char *endp; long long ll = strtoll(tmp, &endp, 10);
    /* Integers beyond int64 are rejected rather than silently rounded. */
    if ((ll == LLONG_MAX || ll == LLONG_MIN) && strcmp(tmp, ll == LLONG_MAX ? "9223372036854775807" : "-9223372036854775808") != 0) {
      st_json_free(v); perr(ps, "integer exceeds int64"); return NULL;
    }
    v->is_int = 1; v->ival = ll;
  }
  return v;
}
static int lit(parser *ps, const char *w) {
  size_t n = strlen(w);
  if ((size_t)(ps->end - ps->p) < n || memcmp(ps->p, w, n) != 0) return 0;
  ps->p += n; return 1;
}
static st_json *parse_value(parser *ps, int depth) {
  if (depth > ST_JSON_MAX_DEPTH) { perr(ps, "nesting too deep"); return NULL; }
  ws(ps);
  if (ps->p >= ps->end) { perr(ps, "unexpected end"); return NULL; }
  char c = *ps->p;
  if (c == '{') {
    ps->p++; st_json *o = st_json_object(); ws(ps);
    if (ps->p < ps->end && *ps->p == '}') { ps->p++; return o; }
    while (1) {
      ws(ps);
      if (ps->p >= ps->end || *ps->p != '"') { perr(ps, "expected key"); st_json_free(o); return NULL; }
      st_buf k; st_buf_init(&k);
      if (!parse_string(ps, &k)) { st_buf_free(&k); st_json_free(o); return NULL; }
      if (!k.data) st_buf_put(&k, "", 0);
      ws(ps);
      if (ps->p >= ps->end || *ps->p != ':') { perr(ps, "expected ':'"); st_buf_free(&k); st_json_free(o); return NULL; }
      ps->p++;
      st_json *v = parse_value(ps, depth + 1);
      if (!v) { st_buf_free(&k); st_json_free(o); return NULL; }
      set_n(o, k.data, k.len, v); st_buf_free(&k);
      ws(ps);
      if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
      if (ps->p < ps->end && *ps->p == '}') { ps->p++; return o; }
      perr(ps, "expected ',' or '}'"); st_json_free(o); return NULL;
    }
  }
  if (c == '[') {
    ps->p++; st_json *a = st_json_array(); ws(ps);
    if (ps->p < ps->end && *ps->p == ']') { ps->p++; return a; }
    while (1) {
      st_json *v = parse_value(ps, depth + 1);
      if (!v) { st_json_free(a); return NULL; }
      st_json_push(a, v); ws(ps);
      if (ps->p < ps->end && *ps->p == ',') { ps->p++; continue; }
      if (ps->p < ps->end && *ps->p == ']') { ps->p++; return a; }
      perr(ps, "expected ',' or ']'"); st_json_free(a); return NULL;
    }
  }
  if (c == '"') {
    st_buf s; st_buf_init(&s);
    if (!parse_string(ps, &s)) { st_buf_free(&s); return NULL; }
    st_json *v = st_json_stringn(s.data ? s.data : "", s.len); st_buf_free(&s); return v;
  }
  if (c == '-' || (c >= '0' && c <= '9')) return parse_number(ps);
  if (lit(ps, "true")) return st_json_bool(1);
  if (lit(ps, "false")) return st_json_bool(0);
  if (lit(ps, "null")) return st_json_null();
  perr(ps, "unexpected token"); return NULL;
}

st_json *st_json_parse(const char *text, size_t len, st_error *err) {
  if (len > ST_JSON_MAX_BYTES) { st_fail(err, ST_E_SIZE_LIMIT, -1, "JSON input exceeds %u bytes", ST_JSON_MAX_BYTES); return NULL; }
  parser ps = { text, text + len, text, err, 0 };
  st_json *v = parse_value(&ps, 0);
  if (!v) return NULL;
  ws(&ps);
  if (ps.p != ps.end) { perr(&ps, "trailing data"); st_json_free(v); return NULL; }
  return v;
}

st_json *st_json_parse_file(const char *path, st_error *err) {
  char *d; size_t n;
  if (st_read_file(path, &d, &n, ST_JSON_MAX_BYTES, err)) return NULL;
  st_json *v = st_json_parse(d, n, err); free(d); return v;
}

/* ---------- number formatting ---------- */
/* Shortest round-trip decimal digits and decimal-point position: value = 0.DIGITS x 10^decpt. */
static void shortest(double v, char *digits, int *ndig, int *decpt) {
  char tmp[64];
  for (int prec = 1; prec <= 17; ++prec) {
    snprintf(tmp, sizeof tmp, "%.*e", prec - 1, v);
    if (strtod(tmp, NULL) == v) break;
  }
  /* tmp: d[.ddd]e[+-]XX */
  int n = 0; const char *p = tmp;
  for (; *p && *p != 'e'; ++p) if (*p >= '0' && *p <= '9') digits[n++] = *p;
  int ex = atoi(p + 1);
  while (n > 1 && digits[n-1] == '0') n--;
  digits[n] = '\0'; *ndig = n; *decpt = ex + 1;
}

void st_format_double_python(double v, char out[40]) {
  if (v == 0) { strcpy(out, signbit(v) ? "-0.0" : "0.0"); return; }
  char d[32]; int n, dp; char *o = out;
  if (v < 0) { *o++ = '-'; v = -v; }
  shortest(v, d, &n, &dp);
  if (dp > -4 && dp <= 16) {
    if (dp <= 0) { *o++ = '0'; *o++ = '.'; for (int i = 0; i < -dp; ++i) *o++ = '0'; memcpy(o, d, (size_t)n); o += n; }
    else if (dp >= n) { memcpy(o, d, (size_t)n); o += n; for (int i = n; i < dp; ++i) *o++ = '0'; *o++ = '.'; *o++ = '0'; }
    else { memcpy(o, d, (size_t)dp); o += dp; *o++ = '.'; memcpy(o, d + dp, (size_t)(n - dp)); o += n - dp; }
    *o = '\0';
  } else {
    *o++ = d[0];
    if (n > 1) { *o++ = '.'; memcpy(o, d + 1, (size_t)(n - 1)); o += n - 1; }
    int e = dp - 1;
    sprintf(o, "e%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
  }
}

void st_format_double_js(double v, char out[40]) {
  if (v == 0) { strcpy(out, "0"); return; }
  char d[32]; int k, n; char *o = out;
  if (v < 0) { *o++ = '-'; v = -v; }
  shortest(v, d, &k, &n);
  if (k <= n && n <= 21) { memcpy(o, d, (size_t)k); o += k; for (int i = k; i < n; ++i) *o++ = '0'; *o = '\0'; }
  else if (0 < n && n <= 21) { memcpy(o, d, (size_t)n); o += n; *o++ = '.'; memcpy(o, d + n, (size_t)(k - n)); o += k - n; *o = '\0'; }
  else if (-6 < n && n <= 0) { *o++ = '0'; *o++ = '.'; for (int i = 0; i < -n; ++i) *o++ = '0'; memcpy(o, d, (size_t)k); o += k; *o = '\0'; }
  else {
    *o++ = d[0];
    if (k > 1) { *o++ = '.'; memcpy(o, d + 1, (size_t)(k - 1)); o += k - 1; }
    int e = n - 1;
    sprintf(o, "e%c%d", e < 0 ? '-' : '+', e < 0 ? -e : e);
  }
}

/* ---------- writers ---------- */
static void write_string(st_buf *b, const char *s, size_t n) {
  st_buf_put(b, "\"", 1);
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '"': st_buf_puts(b, "\\\""); break; case '\\': st_buf_puts(b, "\\\\"); break;
      case '\b': st_buf_puts(b, "\\b"); break; case '\f': st_buf_puts(b, "\\f"); break;
      case '\n': st_buf_puts(b, "\\n"); break; case '\r': st_buf_puts(b, "\\r"); break;
      case '\t': st_buf_puts(b, "\\t"); break;
      default:
        if (c < 0x20) st_buf_printf(b, "\\u%04x", c); else st_buf_put(b, &s[i], 1);
    }
  }
  st_buf_put(b, "\"", 1);
}

/* UTF-16 code-unit comparison for RFC 8785 key ordering. */
static int cmp_utf16(const char *a, size_t an, const char *b, size_t bn) {
  const unsigned char *pa = (const unsigned char *)a, *ea = pa + an, *pb = (const unsigned char *)b, *eb = pb + bn;
  unsigned qa[2], qb[2]; int na = 0, nb = 0, ia = 0, ib = 0;
  while (1) {
    if (ia == na) { if (pa >= ea) { na = 0; } else { unsigned cp; size_t l = utf8_decode(pa, ea, &cp); pa += l ? l : 1;
      if (cp >= 0x10000) { cp -= 0x10000; qa[0] = 0xD800 + (cp >> 10); qa[1] = 0xDC00 + (cp & 0x3FF); na = 2; } else { qa[0] = cp; na = 1; } } ia = 0; }
    if (ib == nb) { if (pb >= eb) { nb = 0; } else { unsigned cp; size_t l = utf8_decode(pb, eb, &cp); pb += l ? l : 1;
      if (cp >= 0x10000) { cp -= 0x10000; qb[0] = 0xD800 + (cp >> 10); qb[1] = 0xDC00 + (cp & 0x3FF); nb = 2; } else { qb[0] = cp; nb = 1; } } ib = 0; }
    if (na == 0 || nb == 0) return (na != 0) - (nb != 0);
    if (qa[ia] != qb[ib]) return qa[ia] < qb[ib] ? -1 : 1;
    ia++; ib++;
  }
}
static int cmp_bytes(const char *a, size_t an, const char *b, size_t bn) {
  size_t m = an < bn ? an : bn; int c = memcmp(a, b, m);
  return c ? c : (an > bn) - (an < bn);
}
/* ECMAScript own-property order: array-index keys ascending, then insertion order. */
static int js_index_key(const char *k, size_t n, uint64_t *out) {
  if (n == 0 || n > 10 || (n > 1 && k[0] == '0')) return 0;
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) { if (k[i] < '0' || k[i] > '9') return 0; v = v * 10 + (uint64_t)(k[i] - '0'); }
  if (v >= 4294967295ull) return 0;
  *out = v; return 1;
}

static void order_keys(const st_json *o, st_json_style style, size_t *idx) {
  for (size_t i = 0; i < o->count; ++i) idx[i] = i;
  for (size_t i = 1; i < o->count; ++i) {  /* insertion sort: stable, small objects */
    size_t x = idx[i], j = i;
    while (j > 0) {
      size_t y = idx[j-1]; int before;
      if (style == ST_JSON_PY_CANONICAL) before = cmp_bytes(o->keys[x], o->klens[x], o->keys[y], o->klens[y]) < 0;
      else if (style == ST_JSON_RFC8785) before = cmp_utf16(o->keys[x], o->klens[x], o->keys[y], o->klens[y]) < 0;
      else { uint64_t a, b; int ia = js_index_key(o->keys[x], o->klens[x], &a), ib = js_index_key(o->keys[y], o->klens[y], &b);
             before = ia && (!ib || a < b); }
      if (!before) break;
      idx[j] = y; j--;
    }
    idx[j] = x;
  }
}

static int write_value(st_buf *b, const st_json *v, st_json_style style, int indent) {
  char num[40];
  switch (v->type) {
    case ST_JNULL: st_buf_puts(b, "null"); return 1;
    case ST_JBOOL: st_buf_puts(b, v->boolean ? "true" : "false"); return 1;
    case ST_JNUM:
      if (!isfinite(v->num)) return 0;
      if (v->is_int && style == ST_JSON_PY_CANONICAL) { st_buf_printf(b, "%lld", (long long)v->ival); return 1; }
      if (style == ST_JSON_PY_CANONICAL) st_format_double_python(v->num, num); else st_format_double_js(v->num, num);
      st_buf_puts(b, num); return 1;
    case ST_JSTR: write_string(b, v->str, v->len); return 1;
    case ST_JARR: case ST_JOBJ: {
      int obj = v->type == ST_JOBJ;
      if (v->count == 0) { st_buf_puts(b, obj ? "{}" : "[]"); return 1; }
      size_t *idx = NULL;
      if (obj) { idx = malloc(v->count * sizeof *idx); if (!idx) return 0; order_keys(v, style, idx); }
      st_buf_put(b, obj ? "{" : "[", 1);
      for (size_t i = 0; i < v->count; ++i) {
        size_t k = obj ? idx[i] : i;
        if (i) st_buf_put(b, ",", 1);
        if (style == ST_JSON_JS_PRETTY2) { st_buf_put(b, "\n", 1); for (int s = 0; s < indent + 2; ++s) st_buf_put(b, " ", 1); }
        if (obj) { write_string(b, v->keys[k], v->klens[k]); st_buf_puts(b, style == ST_JSON_JS_PRETTY2 ? ": " : ":"); }
        if (!write_value(b, v->items[k], style, indent + 2)) { free(idx); return 0; }
      }
      if (style == ST_JSON_JS_PRETTY2) { st_buf_put(b, "\n", 1); for (int s = 0; s < indent; ++s) st_buf_put(b, " ", 1); }
      st_buf_put(b, obj ? "}" : "]", 1);
      free(idx); return 1;
    }
  }
  return 0;
}

int st_json_write(st_buf *out, const st_json *v, st_json_style style) {
  return write_value(out, v, style, 0) && !out->failed;
}
