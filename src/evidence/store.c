#define _POSIX_C_SOURCE 200809L
#include "skytrace/evidence.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

st_code st_store_open(st_store *s, const char *root, st_error *err) {
  if (strlen(root) >= sizeof s->root - 200) return st_fail(err, ST_E_IO, -1, "root path too long");
  strcpy(s->root, root);
  return ST_E_OK;
}

st_code st_sha256_file(const char *path, char hex[65], uint64_t *bytes, st_error *err) {
  FILE *f = fopen(path, "rb");
  if (!f) return st_fail(err, ST_E_IO, -1, "ENOENT: no such file or directory, open '%s'", path);
  st_sha256_ctx c; st_sha256_init(&c);
  unsigned char buf[65536]; size_t n; uint64_t total = 0;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) { st_sha256_update(&c, buf, n); total += n; }
  int bad = ferror(f); fclose(f);
  if (bad) return st_fail(err, ST_E_IO, -1, "read failed: %s", path);
  uint8_t d[32]; st_sha256_final(&c, d); st_hex_encode(d, 32, hex);
  if (bytes) *bytes = total;
  return ST_E_OK;
}

st_code st_store_put_file(const st_store *s, const char *src, const char *ext, char hex[65], uint64_t *bytes,
                          char *rel, size_t rel_cap, st_error *err) {
  st_code rc = st_sha256_file(src, hex, bytes, err);
  if (rc) return rc;
  if ((size_t)snprintf(rel, rel_cap, "data/raw/%s.%s", hex, ext) >= rel_cap) return st_fail(err, ST_E_IO, -1, "path too long");
  char dir[4300], dest[4400];
  snprintf(dir, sizeof dir, "%s/data/raw", s->root);
  snprintf(dest, sizeof dest, "%s/%s", s->root, rel);
  if ((rc = st_mkdir_p(dir, err))) return rc;
  if (st_file_exists(dest)) {
    /* Immutable store: never overwrite; the existing object must be byte-identical by digest. */
    char have[65]; uint64_t hb;
    if ((rc = st_sha256_file(dest, have, &hb, err))) return rc;
    if (strcmp(have, hex) != 0 || hb != *bytes) return st_fail(err, ST_E_DIGEST, -1, "Stored object %s is corrupt", rel);
    return ST_E_OK;
  }
  char *data; size_t len;
  if ((rc = st_read_file(src, &data, &len, (size_t)1 << 32, err))) return rc;
  char check[65]; st_sha256_hex(data, len, check);
  if (strcmp(check, hex) != 0) { free(data); return st_fail(err, ST_E_DIGEST, -1, "source changed while ingesting"); }
  rc = st_write_file_atomic(dest, data, len, err);
  free(data); return rc;
}

st_code st_store_verify(const st_store *s, const char *rel, const char *hex, uint64_t bytes, st_error *err) {
  char path[4400]; snprintf(path, sizeof path, "%s/%s", s->root, rel);
  char have[65]; uint64_t n;
  st_code rc = st_sha256_file(path, have, &n, err);
  if (rc) return rc;
  if (strcmp(have, hex) != 0) return st_fail(err, ST_E_DIGEST, -1, "Hash mismatch %s", rel);
  if (n != bytes) return st_fail(err, ST_E_DIGEST, -1, "Byte-count mismatch %s", rel);
  return ST_E_OK;
}

void st_now_iso8601_ms(char out[64]) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm; gmtime_r(&ts.tv_sec, &tm);
  snprintf(out, 64, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000L);
}
