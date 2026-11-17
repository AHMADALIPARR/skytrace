#ifndef SKYTRACE_CORE_H
#define SKYTRACE_CORE_H
/* Core runtime: SHA-256, growable byte buffers, a strict JSON value model with three
 * deterministic writers, three-valued metadata, and the versioned binary interchange
 * buffer read by the BQN engine (bqn/interchange.bqn). */
#include <stddef.h>
#include <stdint.h>
#include "skytrace/error.h"

/* ---------- SHA-256 (FIPS 180-4) ---------- */
typedef struct { uint32_t h[8]; uint64_t bits; uint8_t block[64]; size_t used; } st_sha256_ctx;
void st_sha256_init(st_sha256_ctx *c);
void st_sha256_update(st_sha256_ctx *c, const void *data, size_t n);
void st_sha256_final(st_sha256_ctx *c, uint8_t out[32]);
void st_sha256_hex(const void *data, size_t n, char out[65]);
void st_hex_encode(const uint8_t *bytes, size_t n, char *out); /* lowercase, NUL-terminated */
int st_is_lower_hex(const char *s, size_t n);                  /* exactly n chars of [0-9a-f] */

/* ---------- growable buffer ---------- */
typedef struct { char *data; size_t len, cap; int failed; } st_buf;
void st_buf_init(st_buf *b);
void st_buf_free(st_buf *b);
void st_buf_put(st_buf *b, const void *p, size_t n);
void st_buf_puts(st_buf *b, const char *s);
void st_buf_printf(st_buf *b, const char *fmt, ...);

/* ---------- three-valued metadata: never interpret unknown as false or zero ---------- */
typedef enum { ST_UNKNOWN = -1, ST_KNOWN_FALSE = 0, ST_KNOWN_TRUE = 1 } st_tri;
st_tri st_tri_and(st_tri a, st_tri b);   /* Kleene strong conjunction */
st_tri st_tri_or(st_tri a, st_tri b);
st_tri st_tri_not(st_tri a);
const char *st_tri_name(st_tri t);       /* "known_true" | "known_false" | "unknown" */
/* Same 32-bit predicates as kernel/src/skytrace.c, re-proved in proofs/saw/rewrite.saw. */
int32_t st_case_incomplete(int32_t range, int32_t corroborated);
int32_t st_case_active(int32_t range, int32_t corroborated, int32_t resolution);
int32_t st_case_active_strict(int32_t range, int32_t corroborated, int32_t resolution);

/* ---------- JSON ---------- */
typedef enum { ST_JNULL, ST_JBOOL, ST_JNUM, ST_JSTR, ST_JARR, ST_JOBJ } st_jtype;
typedef struct st_json st_json;
struct st_json {
  st_jtype type;
  int boolean;
  double num;
  int is_int;          /* lexeme had no fraction/exponent and fits int64 (Python int semantics) */
  int64_t ival;
  char *str; size_t len;           /* ST_JSTR: UTF-8, may contain NUL */
  size_t count, cap;               /* ST_JARR / ST_JOBJ */
  st_json **items;
  char **keys; size_t *klens;      /* ST_JOBJ: insertion order, duplicate key keeps first slot, last value */
};
#define ST_JSON_MAX_DEPTH 128
#define ST_JSON_MAX_BYTES (64u * 1024u * 1024u)
st_json *st_json_parse(const char *text, size_t len, st_error *err);
st_json *st_json_parse_file(const char *path, st_error *err);
void st_json_free(st_json *v);
st_json *st_json_get(const st_json *obj, const char *key);  /* NULL if absent or not an object */
int st_json_has(const st_json *obj, const char *key);
st_json *st_json_null(void);
st_json *st_json_bool(int b);
st_json *st_json_int(int64_t v);
st_json *st_json_double(double v);
st_json *st_json_string(const char *s);
st_json *st_json_stringn(const char *s, size_t n);
st_json *st_json_array(void);
st_json *st_json_object(void);
int st_json_push(st_json *arr, st_json *v);                    /* takes ownership */
int st_json_set(st_json *obj, const char *key, st_json *v);    /* takes ownership; replaces in place */
st_json *st_json_clone(const st_json *v);
int st_json_equal(const st_json *a, const st_json *b);

typedef enum {
  ST_JSON_PY_CANONICAL,  /* json.dumps(sort_keys=True, separators=(',',':'), ensure_ascii=False) */
  ST_JSON_JS_PRETTY2,    /* JSON.stringify(v, null, 2) with insertion order */
  ST_JSON_RFC8785        /* JCS: UTF-16 key order, ECMAScript number form, no whitespace */
} st_json_style;
int st_json_write(st_buf *out, const st_json *v, st_json_style style);
void st_format_double_python(double v, char out[40]);
void st_format_double_js(double v, char out[40]);

/* ---------- versioned binary interchange buffer (consumed by bqn/interchange.bqn) ----------
 * Layout, all little-endian:
 *   bytes 0..7   magic "STBUF\0\0\x01" (last byte = format version 1)
 *   bytes 8..11  uint32 element type: 1 = IEEE-754 binary64, 2 = int64
 *   bytes 12..15 uint32 rank r (0..8)
 *   bytes 16..   r x uint64 shape, row-major
 *   then         prod(shape) elements
 * Complex arrays are stored with a trailing axis of length 2 (re, im).
 * Ownership: st_ibuf owns `data`; st_ibuf_free releases it. A written file is immutable
 * once st_ibuf_write returns (temp file + fsync + rename). */
#define ST_IBUF_F64 1u
#define ST_IBUF_I64 2u
#define ST_IBUF_MAX_RANK 8u
typedef struct { uint32_t etype, rank; uint64_t shape[ST_IBUF_MAX_RANK]; size_t count; void *data; } st_ibuf;
st_code st_ibuf_make(st_ibuf *b, uint32_t etype, uint32_t rank, const uint64_t *shape, st_error *err);
void st_ibuf_free(st_ibuf *b);
st_code st_ibuf_write(const st_ibuf *b, const char *path, st_error *err);
st_code st_ibuf_read(st_ibuf *b, const char *path, st_error *err);


/* ---------- explicit dense linear-algebra kernels (row-major, n <= 64) ---------- */
/* Cyclic Jacobi eigen-decomposition of a real symmetric n x n matrix a (destroyed).
 * Eigenvalues ascending in w; eigenvectors as columns of v (may be NULL). Converges when the
 * off-diagonal Frobenius norm <= 1e-15 * ||a||_F or fails with E_NOT_CONVERGED after 100 sweeps. */
st_code st_sym_eigen(double *a, size_t n, double *w, double *v, st_error *err);
/* Solves (a) x = b for symmetric positive-definite a via Cholesky; a, b untouched. */
st_code st_cholesky_solve(const double *a, size_t n, const double *b, double *x, st_error *err);
/* Inverse of a symmetric positive-definite matrix via Cholesky. */
st_code st_spd_inverse(const double *a, size_t n, double *inv, st_error *err);

/* ---------- files ---------- */
st_code st_read_file(const char *path, char **data, size_t *len, size_t max_bytes, st_error *err);
/* Atomic replace: temp file in same dir, fsync, rename, fsync directory. */
st_code st_write_file_atomic(const char *path, const void *data, size_t len, st_error *err);
st_code st_mkdir_p(const char *path, st_error *err);
int st_file_exists(const char *path);

#endif
