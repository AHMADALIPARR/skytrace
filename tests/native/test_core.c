// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/core.h"
#include "check.h"
/* The original kernel is compiled in unchanged to prove the rewrite's predicates are identical. */
#include "../../kernel/src/skytrace.c"

static char *emit(const st_json *v, st_json_style s) { static char out[4096]; st_buf b; st_buf_init(&b); st_json_write(&b, v, s); snprintf(out, sizeof out, "%s", b.data ? b.data : ""); st_buf_free(&b); return out; }
static char *roundtrip(const char *text, st_json_style s) { st_error e; st_json *v = st_json_parse(text, strlen(text), &e); if (!v) return "PARSE-ERROR"; char *o = emit(v, s); st_json_free(v); return o; }
static int rejects(const char *text) { st_error e; st_json *v = st_json_parse(text, strlen(text), &e); if (v) { st_json_free(v); return 0; } return e.code == ST_E_JSON || e.code == ST_E_SIZE_LIMIT; }
static const char *py(double d) { static char b[40]; st_format_double_python(d, b); return b; }
static const char *js(double d) { static char b[40]; st_format_double_js(d, b); return b; }

int main(void) {
  char hex[65];
  st_sha256_hex("abc", 3, hex); CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  st_sha256_hex("", 0, hex); CHECK_STR(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  st_sha256_hex(two, strlen(two), hex); CHECK_STR(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  { st_sha256_ctx c; uint8_t d[32]; char block[1000]; memset(block, 'a', sizeof block); st_sha256_init(&c);
    for (int i = 0; i < 1000; ++i) st_sha256_update(&c, block, sizeof block);
    st_sha256_final(&c, d); st_hex_encode(d, 32, hex); CHECK_STR(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"); }

  /* Python repr / ECMAScript Number#toString shortest round-trip forms */
  CHECK_STR(py(0.1), "0.1"); CHECK_STR(py(1.0), "1.0"); CHECK_STR(py(-0.0), "-0.0"); CHECK_STR(py(1e-05), "1e-05");
  CHECK_STR(py(0.0001), "0.0001"); CHECK_STR(py(1e16), "1e+16"); CHECK_STR(py(1e15), "1000000000000000.0");
  CHECK_STR(py(0.020000000000000018), "0.020000000000000018"); CHECK_STR(py(1.5700924586837752e-16), "1.5700924586837752e-16");
  CHECK_STR(py(123456789012345678.0), "1.2345678901234568e+17"); CHECK_STR(py(2.5), "2.5");
  CHECK_STR(js(1e21), "1e+21"); CHECK_STR(js(1e20), "100000000000000000000"); CHECK_STR(js(1e-7), "1e-7");
  CHECK_STR(js(0.000001), "0.000001"); CHECK_STR(js(100), "100"); CHECK_STR(js(-0.0), "0"); CHECK_STR(js(1.5), "1.5");

  CHECK_STR(roundtrip("{\"b\":1,\"a\":[2.0,null,true,\"x\\u00e9\\n\"]}", ST_JSON_PY_CANONICAL), "{\"a\":[2.0,null,true,\"x\xc3\xa9\\n\"],\"b\":1}");
  CHECK_STR(roundtrip("{\"b\":1,\"a\":{}, \"c\":[]}", ST_JSON_JS_PRETTY2), "{\n  \"b\": 1,\n  \"a\": {},\n  \"c\": []\n}");
  CHECK_STR(roundtrip("{\"x\":1,\"2\":1,\"1\":1}", ST_JSON_JS_PRETTY2), "{\n  \"1\": 1,\n  \"2\": 1,\n  \"x\": 1\n}");
  /* RFC 8785 section 3.2.3 ordering example (UTF-16 code units) */
  CHECK_STR(roundtrip("{\"\\u20ac\":1,\"\\r\":2,\"\\ufb33\":3,\"1\":4,\"\\ud83d\\ude00\":5,\"\\u0080\":6,\"\\u00f6\":7}", ST_JSON_RFC8785),
            "{\"\\r\":2,\"1\":4,\"\xc2\x80\":6,\"\xc3\xb6\":7,\"\xe2\x82\xac\":1,\"\xf0\x9f\x98\x80\":5,\"\xef\xac\xb3\":3}");
  CHECK_STR(roundtrip("{\"a\":1,\"a\":2}", ST_JSON_PY_CANONICAL), "{\"a\":2}");
  CHECK_STR(roundtrip("[1E400]", ST_JSON_PY_CANONICAL), "PARSE-ERROR");
  CHECK(rejects("NaN")); CHECK(rejects("[1,]")); CHECK(rejects("{\"a\" 1}")); CHECK(rejects("\"\\ud800\""));
  CHECK(rejects("\"\xff\"")); CHECK(rejects("\"\xc0\xaf\"")); CHECK(rejects("01")); CHECK(rejects("[1] x"));
  CHECK(rejects("99999999999999999999")); CHECK(rejects("\"a\tb\""));
  { char deep[400]; memset(deep, '[', 300); deep[300] = '\0'; CHECK(rejects(deep)); }

  /* Kleene three-valued logic */
  st_tri v[3] = {ST_UNKNOWN, ST_KNOWN_FALSE, ST_KNOWN_TRUE};
  for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
    st_tri a = v[i], b = v[j];
    CHECK(st_tri_and(a, b) == st_tri_not(st_tri_or(st_tri_not(a), st_tri_not(b))));  /* De Morgan */
    CHECK(st_tri_and(a, b) == st_tri_and(b, a));
  }
  CHECK(st_tri_and(ST_UNKNOWN, ST_KNOWN_FALSE) == ST_KNOWN_FALSE);
  CHECK(st_tri_and(ST_UNKNOWN, ST_KNOWN_TRUE) == ST_UNKNOWN);
  CHECK_STR(st_tri_name(ST_UNKNOWN), "unknown");
  /* Rewritten predicates equal the original kernel on the whole ternary domain and on wide samples. */
  for (int r = -1; r <= 1; ++r) for (int c = -1; c <= 1; ++c) for (int s = -1; s <= 1; ++s) {
    CHECK(st_case_active(r, c, s) == st_active(r, c, s));
    CHECK(st_case_incomplete(r, c) == st_incomplete(r, c));
    CHECK(st_case_active_strict(r, c, s) == (s == 0 && (r != 1 || c != 1)));
  }
  uint32_t x = 2463534242u; int mism = 0;
  for (int i = 0; i < 100000; ++i) {
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; int32_t r = (int32_t)x;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; int32_t c = (int32_t)(x % 4) - 1;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; int32_t s = (int32_t)x;
    mism += st_case_active(r, c, s) != st_active(r, c, s);
  }
  CHECK(mism == 0);

  /* Interchange buffer round trip */
  { st_ibuf b, c; st_error e; uint64_t sh[2] = {2, 3};
    CHECK(st_ibuf_make(&b, ST_IBUF_F64, 2, sh, &e) == ST_E_OK);
    for (int i = 0; i < 6; ++i) ((double *)b.data)[i] = i * 0.5 - 1;
    CHECK(st_ibuf_write(&b, "test_core.stbuf", &e) == ST_E_OK);
    CHECK(st_ibuf_read(&c, "test_core.stbuf", &e) == ST_E_OK);
    CHECK(c.rank == 2 && c.shape[0] == 2 && c.shape[1] == 3 && c.etype == ST_IBUF_F64);
    CHECK(memcmp(b.data, c.data, 48) == 0);
    st_ibuf_free(&b); st_ibuf_free(&c);
    FILE *f = fopen("test_core.stbuf", "ab"); fputc(0, f); fclose(f);
    CHECK(st_ibuf_read(&c, "test_core.stbuf", &e) == ST_E_SHAPE);
    remove("test_core.stbuf"); }

  /* Dense kernels */
  { double a[4] = {2, 1, 1, 2}, w[2], vv[4]; st_error e;
    CHECK(st_sym_eigen(a, 2, w, vv, &e) == ST_E_OK); CHECK_NEAR(w[0], 1, 1e-14); CHECK_NEAR(w[1], 3, 1e-14);
    CHECK_NEAR(fabs(vv[0]), sqrt(0.5), 1e-14);
    double s[9] = {4, 12, -16, 12, 37, -43, -16, -43, 98}, b[3] = {1, 2, 3}, xs[3];
    CHECK(st_cholesky_solve(s, 3, b, xs, &e) == ST_E_OK);
    for (int i = 0; i < 3; ++i) CHECK_NEAR(s[3*i] * xs[0] + s[3*i+1] * xs[1] + s[3*i+2] * xs[2], b[i], 1e-10);
    double sing[4] = {1, 1, 1, 1}; CHECK(st_cholesky_solve(sing, 2, b, xs, &e) == ST_E_SINGULAR);
    double asym[4] = {1, 2, 3, 4}; CHECK(st_sym_eigen(asym, 2, w, NULL, &e) == ST_E_SHAPE); }
  return finish("test_core");
}
