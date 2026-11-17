// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* JS-compatible evidence commands: sources | ingest | cases | verify (bin/skytrace.mjs contract). */
#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include "skytrace/evidence.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static st_code read_json(const char *path, st_json **out, st_error *err) {
  *out = st_json_parse_file(path, err);
  return *out ? ST_E_OK : (err->code ? err->code : ST_E_JSON);
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Lists *.json names in dir, sorted by byte order (= UTF-16 order for ASCII names). */
static st_code list_json(const char *dir, char ***names, size_t *n, st_error *err) {
  DIR *d = opendir(dir);
  if (!d) return st_fail(err, ST_E_IO, -1, "ENOENT: no such file or directory, scandir '%s'", dir);
  size_t cap = 16; *n = 0; *names = malloc(cap * sizeof **names);
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t l = strlen(e->d_name);
    if (l < 5 || strcmp(e->d_name + l - 5, ".json") != 0) continue;
    if (*n == cap) { cap *= 2; *names = realloc(*names, cap * sizeof **names); }
    (*names)[(*n)++] = strdup(e->d_name);
  }
  closedir(d);
  qsort(*names, *n, sizeof **names, cmp_str);
  return ST_E_OK;
}
static void free_names(char **names, size_t n) { for (size_t i = 0; i < n; ++i) free(names[i]); free(names); }

static void print_js_string(const st_json *v) {
  st_buf b; st_buf_init(&b);
  if (v->type == ST_JSTR) st_buf_put(&b, v->str, v->len);
  else st_json_write(&b, v, ST_JSON_JS_PRETTY2);
  fwrite(b.data ? b.data : "", 1, b.len, stdout); st_buf_free(&b);
}

st_code cli_sources(const char *root, st_error *err) {
  char path[4400]; snprintf(path, sizeof path, "%s/data/sources.json", root);
  st_json *r; st_code rc = read_json(path, &r, err);
  if (rc) return rc;
  st_json *s = st_json_get(r, "sources");
  if (!s || s->type != ST_JARR) { st_json_free(r); return st_fail(err, ST_E_SCHEMA, -1, "registry.sources is not iterable"); }
  for (size_t i = 0; i < s->count; ++i) {
    const char *f[3] = {"id", "tier", "url"};
    for (int k = 0; k < 3; ++k) {
      st_json *v = st_json_get(s->items[i], f[k]);
      if (v) print_js_string(v); else fputs("undefined", stdout);
      fputc(k < 2 ? '\t' : '\n', stdout);
    }
  }
  st_json_free(r); return ST_E_OK;
}

st_code cli_cases(const char *root, st_error *err) {
  char dir[4400]; snprintf(dir, sizeof dir, "%s/data/cases", root);
  char **names; size_t n; st_code rc = list_json(dir, &names, &n, err);
  if (rc) return rc;
  for (size_t i = 0; i < n; ++i) {
    char path[4800]; snprintf(path, sizeof path, "%s/%s", dir, names[i]);
    st_json *c; if ((rc = read_json(path, &c, err))) break;
    if ((rc = st_validate_case(c, err))) { st_json_free(c); break; }
    const st_json *o = st_json_get(c, "observed_at");
    print_js_string(st_json_get(c, "case_id")); fputc('\t', stdout);
    print_js_string(st_json_get(c, "source_id")); fputc('\t', stdout);
    print_js_string(st_json_get(c, "classification")); fputc('\t', stdout);
    if (o->type == ST_JNULL) fputs("unknown", stdout); else print_js_string(o);
    fputc('\n', stdout);
    st_json_free(c);
  }
  if (!rc) printf("Total: %zu\n", n);
  free_names(names, n); return rc;
}

st_code cli_verify(const char *root, st_error *err) {
  char dir[4400]; snprintf(dir, sizeof dir, "%s/data/cases", root);
  char **names; size_t n; st_code rc = list_json(dir, &names, &n, err);
  if (rc) return rc;
  st_store store; st_store_open(&store, root, err);
  size_t count = 0;
  for (size_t i = 0; i < n && !rc; ++i) {
    char path[4800]; snprintf(path, sizeof path, "%s/%s", dir, names[i]);
    st_json *c; if ((rc = read_json(path, &c, err))) break;
    if ((rc = st_validate_case(c, err))) { st_json_free(c); break; }
    const st_json *ev = st_json_get(c, "evidence");
    for (size_t k = 0; k < ev->count && !rc; ++k) {
      const st_json *e = ev->items[k], *rp = st_json_get(e, "relative_path"), *h = st_json_get(e, "sha256"), *b = st_json_get(e, "bytes");
      char p2[4800]; snprintf(p2, sizeof p2, "%s/%s", root, rp->str);
      char have[65]; uint64_t nb;
      if ((rc = st_sha256_file(p2, have, &nb, err))) break;
      if (strcmp(have, h->str) != 0) { rc = st_fail(err, ST_E_DIGEST, (long)k, "Hash mismatch %s", rp->str); break; }
      if (!b || b->type != ST_JNUM || b->num != (double)nb) { rc = st_fail(err, ST_E_DIGEST, (long)k, "Byte-count mismatch %s", rp->str); break; }
      count++;
    }
    st_json_free(c);
  }
  free_names(names, n);
  if (rc) return rc;
  /* The append-only journal, when present, must form an unbroken hash chain. */
  size_t events; char last[65];
  if ((rc = st_journal_verify(root, &events, last, err))) return rc;
  printf("PASS: %zu cases, %zu evidence files independently checked\n", n, count);
  return ST_E_OK;
}

static int is_http_url(const char *u) { return strncmp(u, "http://", 7) == 0 || strncmp(u, "https://", 8) == 0; }

st_code cli_ingest(const char *root, int argc, char **argv, st_error *err) {
  const char *sid = argc > 0 ? argv[0] : NULL, *rid = argc > 1 ? argv[1] : NULL, *file = argc > 2 ? argv[2] : NULL, *url = argc > 3 ? argv[3] : NULL;
  char path[4400]; snprintf(path, sizeof path, "%s/data/sources.json", root);
  st_json *reg; st_code rc = read_json(path, &reg, err);
  if (rc) return rc;
  st_json *srcs = st_json_get(reg, "sources"); int known = 0;
  for (size_t i = 0; sid && srcs && srcs->type == ST_JARR && i < srcs->count; ++i) {
    st_json *id = st_json_get(srcs->items[i], "id");
    if (id && id->type == ST_JSTR && id->len == strlen(sid) && memcmp(id->str, sid, id->len) == 0) known = 1;
  }
  st_json_free(reg);
  if (!known) return st_fail(err, ST_E_SOURCE_POLICY, -1, "Unknown source ID");
  if (!(rid && *rid && file && *file && url && *url && is_http_url(url)))
    return st_fail(err, ST_E_ARGUMENT, -1, "Usage: ingest <source-id> <source-record-id> <local-file> <original-url>");
  /* ext = basename(file).split('.').pop() || 'bin', lowercased (JS semantics, including no-dot names). */
  const char *base = strrchr(file, '/'); base = base ? base + 1 : file;
  const char *dot = strrchr(base, '.'); const char *e0 = dot ? dot + 1 : base;
  char ext[64]; size_t el = strlen(e0);
  if (el == 0) { strcpy(ext, "bin"); el = 3; }
  else { if (el >= sizeof ext) el = sizeof ext - 1; for (size_t i = 0; i < el; ++i) ext[i] = (char)(e0[i] >= 'A' && e0[i] <= 'Z' ? e0[i] + 32 : e0[i]); ext[el] = '\0'; }
  int ext_ok = el >= 1 && el <= 8 && strlen(e0 ? e0 : "") <= 8;
  for (size_t i = 0; ext_ok && i < el; ++i) if (!((ext[i] >= 'a' && ext[i] <= 'z') || (ext[i] >= '0' && ext[i] <= '9'))) ext_ok = 0;
  if (!st_file_exists(file)) {
    char cwd[4096] = "";
    if (file[0] != '/' && !getcwd(cwd, sizeof cwd)) cwd[0] = '\0';
    return st_fail(err, ST_E_IO, -1, "ENOENT: no such file or directory, open '%s%s%s'", cwd, cwd[0] ? "/" : "", file);
  }
  if (!ext_ok) return st_fail(err, ST_E_ARGUMENT, -1, "Unsupported file extension");
  st_store store; st_store_open(&store, root, err);
  char hex[65], rel[200]; uint64_t bytes;
  if ((rc = st_store_put_file(&store, file, ext, hex, &bytes, rel, sizeof rel, err))) return rc;
  char id[20]; st_stable_case_id(sid, strlen(sid), rid, strlen(rid), id);
  char cdir[4400], cfile[4500]; snprintf(cdir, sizeof cdir, "%s/data/cases", root); snprintf(cfile, sizeof cfile, "%s/%s.json", cdir, id);
  st_json *c;
  if (st_file_exists(cfile)) { if ((rc = read_json(cfile, &c, err))) return rc; }
  else {
    c = st_json_object();
    st_json_set(c, "case_id", st_json_string(id)); st_json_set(c, "source_id", st_json_string(sid));
    st_json_set(c, "source_record_id", st_json_string(rid)); st_json_set(c, "observed_at", st_json_null());
    st_json_set(c, "time_precision", st_json_string("unknown")); st_json_set(c, "classification", st_json_string("unassessed"));
    st_json_set(c, "evidence", st_json_array()); st_json_set(c, "claims", st_json_array());
  }
  st_json *ev = st_json_get(c, "evidence");
  if (!ev || ev->type != ST_JARR) { st_json_free(c); return st_fail(err, ST_E_SCHEMA, -1, "Evidence/claims must be arrays"); }
  for (size_t i = 0; i < ev->count; ++i) {
    st_json *rp = st_json_get(ev->items[i], "relative_path");
    if (rp && rp->type == ST_JSTR && strcmp(rp->str, rel) == 0) { st_json_free(c); return st_fail(err, ST_E_CONFLICT, (long)i, "Evidence already linked to case"); }
  }
  char now[64]; st_now_iso8601_ms(now);
  st_json *entry = st_json_object();
  st_json_set(entry, "sha256", st_json_string(hex)); st_json_set(entry, "relative_path", st_json_string(rel));
  st_json_set(entry, "source_url", st_json_string(url)); st_json_set(entry, "ingested_at", st_json_string(now));
  st_json_set(entry, "bytes", st_json_int((int64_t)bytes));
  st_json_push(ev, entry);
  if ((rc = st_validate_case(c, err))) { st_json_free(c); return rc; }
  if ((rc = st_mkdir_p(cdir, err))) { st_json_free(c); return rc; }
  st_buf out; st_buf_init(&out); st_json_write(&out, c, ST_JSON_JS_PRETTY2); st_buf_put(&out, "\n", 1);
  rc = st_write_file_atomic(cfile, out.data, out.len, err);
  st_buf_free(&out); st_json_free(c);
  if (rc) return rc;
  st_json *payload = st_json_object();
  st_json_set(payload, "bytes", st_json_int((int64_t)bytes)); st_json_set(payload, "case_id", st_json_string(id));
  st_json_set(payload, "relative_path", st_json_string(rel)); st_json_set(payload, "sha256", st_json_string(hex));
  st_json_set(payload, "source_url", st_json_string(url));
  rc = st_journal_append(root, "skytrace-cli", "evidence_ingested", payload, now, NULL, err);
  st_json_free(payload);
  if (rc) return rc;
  st_json *res = st_json_object();
  char cf[64]; snprintf(cf, sizeof cf, "data/cases/%s.json", id);
  st_json_set(res, "case_id", st_json_string(id)); st_json_set(res, "sha256", st_json_string(hex));
  st_json_set(res, "case_file", st_json_string(cf)); st_json_set(res, "evidence_file", st_json_string(rel));
  st_buf o; st_buf_init(&o); st_json_write(&o, res, ST_JSON_JS_PRETTY2);
  printf("%s\n", o.data); st_buf_free(&o); st_json_free(res);
  return ST_E_OK;
}
