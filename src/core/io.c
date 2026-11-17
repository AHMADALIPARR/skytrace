#define _POSIX_C_SOURCE 200809L
#include "skytrace/core.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void st_buf_init(st_buf *b) { b->data = NULL; b->len = b->cap = 0; b->failed = 0; }
void st_buf_free(st_buf *b) { free(b->data); st_buf_init(b); }
void st_buf_put(st_buf *b, const void *p, size_t n) {
  if (b->failed) return;
  if (b->len + n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + n + 1) { if (cap > SIZE_MAX / 2) { b->failed = 1; return; } cap *= 2; }
    char *d = realloc(b->data, cap);
    if (!d) { b->failed = 1; return; }
    b->data = d; b->cap = cap;
  }
  if (n) memcpy(b->data + b->len, p, n);
  b->len += n; b->data[b->len] = '\0';
}
void st_buf_puts(st_buf *b, const char *s) { st_buf_put(b, s, strlen(s)); }
void st_buf_printf(st_buf *b, const char *fmt, ...) {
  char small[512]; va_list ap; va_start(ap, fmt);
  int n = vsnprintf(small, sizeof small, fmt, ap); va_end(ap);
  if (n < 0) { b->failed = 1; return; }
  if ((size_t)n < sizeof small) { st_buf_put(b, small, (size_t)n); return; }
  char *big = malloc((size_t)n + 1);
  if (!big) { b->failed = 1; return; }
  va_start(ap, fmt); vsnprintf(big, (size_t)n + 1, fmt, ap); va_end(ap);
  st_buf_put(b, big, (size_t)n); free(big);
}

int st_file_exists(const char *path) { struct stat st; return stat(path, &st) == 0; }

st_code st_read_file(const char *path, char **data, size_t *len, size_t max_bytes, st_error *err) {
  *data = NULL; *len = 0;
  FILE *f = fopen(path, "rb");
  if (!f) return st_fail(err, ST_E_IO, -1, "ENOENT: no such file or directory, open '%s'", path);
  st_buf b; st_buf_init(&b);
  char chunk[65536]; size_t n;
  while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) {
    if (b.len + n > max_bytes) { fclose(f); st_buf_free(&b); return st_fail(err, ST_E_SIZE_LIMIT, -1, "file exceeds %zu bytes: %s", max_bytes, path); }
    st_buf_put(&b, chunk, n);
  }
  int bad = ferror(f); fclose(f);
  if (bad || b.failed) { st_buf_free(&b); return st_fail(err, ST_E_IO, -1, "read failed: %s", path); }
  if (!b.data) st_buf_put(&b, "", 0);
  *data = b.data; *len = b.len;
  return ST_E_OK;
}

static st_code fsync_dir_of(const char *path, st_error *err) {
  char dir[4096]; size_t n = strlen(path);
  if (n >= sizeof dir) return st_fail(err, ST_E_IO, -1, "path too long");
  memcpy(dir, path, n + 1);
  char *slash = strrchr(dir, '/');
  if (slash) { if (slash == dir) slash[1] = '\0'; else *slash = '\0'; } else strcpy(dir, ".");
  int fd = open(dir, O_RDONLY);
  if (fd < 0) return st_fail(err, ST_E_IO, -1, "open dir failed: %s", dir);
  int rc = fsync(fd); close(fd);
  /* Some file systems reject fsync on directories (EINVAL); that is not a data-loss signal. */
  if (rc != 0 && errno != EINVAL) return st_fail(err, ST_E_IO, -1, "fsync dir failed: %s", dir);
  return ST_E_OK;
}

st_code st_write_file_atomic(const char *path, const void *data, size_t len, st_error *err) {
  char tmp[4200];
  if (snprintf(tmp, sizeof tmp, "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof tmp)
    return st_fail(err, ST_E_IO, -1, "path too long");
  int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return st_fail(err, ST_E_IO, -1, "cannot create %s: %s", tmp, strerror(errno));
  const char *p = data; size_t left = len;
  while (left) {
    ssize_t w = write(fd, p, left);
    if (w < 0) { if (errno == EINTR) continue; close(fd); unlink(tmp); return st_fail(err, ST_E_IO, -1, "write failed: %s", tmp); }
    p += w; left -= (size_t)w;
  }
  if (fsync(fd) != 0) { close(fd); unlink(tmp); return st_fail(err, ST_E_IO, -1, "fsync failed: %s", tmp); }
  close(fd);
  if (rename(tmp, path) != 0) { unlink(tmp); return st_fail(err, ST_E_IO, -1, "rename failed: %s", path); }
  return fsync_dir_of(path, err);
}

st_code st_mkdir_p(const char *path, st_error *err) {
  char p[4096]; size_t n = strlen(path);
  if (n == 0 || n >= sizeof p) return st_fail(err, ST_E_IO, -1, "bad directory path");
  memcpy(p, path, n + 1);
  for (size_t i = 1; i <= n; ++i) {
    if (p[i] == '/' || p[i] == '\0') {
      char save = p[i]; p[i] = '\0';
      if (mkdir(p, 0755) != 0 && errno != EEXIST) return st_fail(err, ST_E_IO, -1, "mkdir failed: %s", p);
      p[i] = save;
    }
  }
  return ST_E_OK;
}

/* ---------- interchange buffers ---------- */
static const unsigned char MAGIC[8] = {'S','T','B','U','F',0,0,1};

st_code st_ibuf_make(st_ibuf *b, uint32_t etype, uint32_t rank, const uint64_t *shape, st_error *err) {
  memset(b, 0, sizeof *b);
  if (etype != ST_IBUF_F64 && etype != ST_IBUF_I64) return st_fail(err, ST_E_ARGUMENT, -1, "unknown element type %u", etype);
  if (rank > ST_IBUF_MAX_RANK) return st_fail(err, ST_E_SHAPE, -1, "rank %u exceeds %u", rank, ST_IBUF_MAX_RANK);
  size_t count = 1;
  for (uint32_t i = 0; i < rank; ++i) {
    if (shape[i] && count > (SIZE_MAX / 8) / shape[i]) return st_fail(err, ST_E_SIZE_LIMIT, (long)i, "shape overflow");
    count *= (size_t)shape[i]; b->shape[i] = shape[i];
  }
  b->etype = etype; b->rank = rank; b->count = count;
  b->data = calloc(count ? count : 1, 8);
  if (!b->data) return st_fail(err, ST_E_NOMEM, -1, "out of memory");
  return ST_E_OK;
}
void st_ibuf_free(st_ibuf *b) { free(b->data); memset(b, 0, sizeof *b); }

static void put_u32(st_buf *o, uint32_t v) { unsigned char c[4]; for (int i = 0; i < 4; ++i) c[i] = (unsigned char)(v >> (8*i)); st_buf_put(o, c, 4); }
static void put_u64(st_buf *o, uint64_t v) { unsigned char c[8]; for (int i = 0; i < 8; ++i) c[i] = (unsigned char)(v >> (8*i)); st_buf_put(o, c, 8); }
static uint64_t get_u64(const unsigned char *p) { uint64_t v = 0; for (int i = 7; i >= 0; --i) v = v << 8 | p[i]; return v; }
static uint32_t get_u32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

st_code st_ibuf_write(const st_ibuf *b, const char *path, st_error *err) {
  st_buf o; st_buf_init(&o);
  st_buf_put(&o, MAGIC, 8); put_u32(&o, b->etype); put_u32(&o, b->rank);
  for (uint32_t i = 0; i < b->rank; ++i) put_u64(&o, b->shape[i]);
  for (size_t i = 0; i < b->count; ++i) {
    uint64_t bits; memcpy(&bits, (const char *)b->data + 8*i, 8); put_u64(&o, bits);
  }
  if (o.failed) { st_buf_free(&o); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  st_code rc = st_write_file_atomic(path, o.data, o.len, err);
  st_buf_free(&o); return rc;
}

st_code st_ibuf_read(st_ibuf *b, const char *path, st_error *err) {
  char *raw; size_t len;
  st_code rc = st_read_file(path, &raw, &len, 1u << 30, err);
  if (rc) return rc;
  const unsigned char *p = (const unsigned char *)raw;
  if (len < 16 || memcmp(p, MAGIC, 8) != 0) { free(raw); return st_fail(err, ST_E_SCHEMA, -1, "bad interchange magic/version"); }
  uint32_t et = get_u32(p + 8), rank = get_u32(p + 12);
  if (rank > ST_IBUF_MAX_RANK || len < 16 + 8u * rank) { free(raw); return st_fail(err, ST_E_SHAPE, -1, "bad rank"); }
  uint64_t shape[ST_IBUF_MAX_RANK];
  for (uint32_t i = 0; i < rank; ++i) shape[i] = get_u64(p + 16 + 8*i);
  rc = st_ibuf_make(b, et, rank, shape, err);
  if (rc) { free(raw); return rc; }
  size_t off = 16 + 8u * rank;
  if (len != off + 8u * b->count) { st_ibuf_free(b); free(raw); return st_fail(err, ST_E_SHAPE, -1, "payload length %zu does not match shape", len - off); }
  for (size_t i = 0; i < b->count; ++i) { uint64_t bits = get_u64(p + off + 8*i); memcpy((char *)b->data + 8*i, &bits, 8); }
  free(raw); return ST_E_OK;
}
