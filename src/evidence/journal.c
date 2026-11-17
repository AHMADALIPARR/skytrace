// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#define _POSIX_C_SOURCE 200809L
#include "skytrace/evidence.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char GENESIS[65] = "0000000000000000000000000000000000000000000000000000000000000000";

static void journal_path(const char *root, char *dir, size_t dn, char *file, size_t fn) {
  snprintf(dir, dn, "%s/data/manifests", root);
  snprintf(file, fn, "%s/data/manifests/events.ndjson", root);
}

/* Verifies events in text[0..len). */
static st_code verify_text(const char *text, size_t len, size_t *count, char last[65], st_error *err) {
  size_t seq = 0; strcpy(last, GENESIS);
  const char *p = text, *end = text + len;
  while (p < end) {
    const char *nl = memchr(p, '\n', (size_t)(end - p));
    if (!nl) return st_fail(err, ST_E_JOURNAL, (long)seq, "journal tail is not newline-terminated (torn write)");
    st_json *ev = st_json_parse(p, (size_t)(nl - p), err);
    if (!ev) return st_fail(err, ST_E_JOURNAL, (long)seq, "journal line %zu is not JSON", seq + 1);
    st_json *s = st_json_get(ev, "event_sha256"), *prev = st_json_get(ev, "prev_event_sha256"), *sq = st_json_get(ev, "seq");
    int ok = s && s->type == ST_JSTR && s->len == 64 && prev && prev->type == ST_JSTR && prev->len == 64 &&
             memcmp(prev->str, last, 64) == 0 && sq && sq->is_int && sq->ival == (int64_t)seq;
    char hash[65] = {0};
    if (ok) {
      memcpy(hash, s->str, 64);
      /* Canonical bytes of the event without its own digest. */
      st_json *body = st_json_object();
      for (size_t i = 0; i < ev->count; ++i) if (strcmp(ev->keys[i], "event_sha256") != 0) st_json_set(body, ev->keys[i], st_json_clone(ev->items[i]));
      st_buf b; st_buf_init(&b); st_json_write(&b, body, ST_JSON_RFC8785);
      char re[65]; st_sha256_hex(b.data, b.len, re);
      /* The stored line must itself be the canonical encoding (no alternate spellings). */
      st_buf full; st_buf_init(&full); st_json_write(&full, ev, ST_JSON_RFC8785);
      ok = strcmp(re, hash) == 0 && full.len == (size_t)(nl - p) && memcmp(full.data, p, full.len) == 0;
      st_buf_free(&b); st_buf_free(&full); st_json_free(body);
    }
    st_json_free(ev);
    if (!ok) return st_fail(err, ST_E_JOURNAL, (long)seq, "journal chain broken at event %zu", seq);
    memcpy(last, hash, 65); seq++; p = nl + 1;
  }
  *count = seq; return ST_E_OK;
}

st_code st_journal_verify(const char *root, size_t *count, char last[65], st_error *err) {
  char dir[4300], file[4400]; journal_path(root, dir, sizeof dir, file, sizeof file);
  *count = 0; strcpy(last, GENESIS);
  if (!st_file_exists(file)) return ST_E_OK;
  char *text; size_t len;
  st_code rc = st_read_file(file, &text, &len, ST_JSON_MAX_BYTES, err);
  if (rc) return rc;
  rc = verify_text(text, len, count, last, err);
  free(text); return rc;
}

st_code st_journal_append(const char *root, const char *actor, const char *kind, const st_json *payload,
                          const char *timestamp, char out_sha[65], st_error *err) {
  char dir[4300], file[4400]; journal_path(root, dir, sizeof dir, file, sizeof file);
  st_code rc = st_mkdir_p(dir, err);
  if (rc) return rc;
  int fd = open(file, O_RDWR | O_CREAT | O_APPEND, 0644);
  if (fd < 0) return st_fail(err, ST_E_IO, -1, "cannot open journal: %s", strerror(errno));
  struct flock lk; memset(&lk, 0, sizeof lk); lk.l_type = F_WRLCK; lk.l_whence = SEEK_SET;
  if (fcntl(fd, F_SETLKW, &lk) != 0) { close(fd); return st_fail(err, ST_E_IO, -1, "journal lock failed"); }
  size_t count; char last[65];
  rc = st_journal_verify(root, &count, last, err);
  if (rc) { close(fd); return rc; }
  st_json *ev = st_json_object();
  st_json_set(ev, "actor", st_json_string(actor));
  st_json_set(ev, "kind", st_json_string(kind));
  st_json_set(ev, "payload", payload ? st_json_clone(payload) : st_json_null());
  st_json_set(ev, "prev_event_sha256", st_json_string(last));
  st_json_set(ev, "seq", st_json_int((int64_t)count));
  st_json_set(ev, "timestamp", st_json_string(timestamp));
  st_buf b; st_buf_init(&b);
  if (!st_json_write(&b, ev, ST_JSON_RFC8785)) { st_buf_free(&b); st_json_free(ev); close(fd); return st_fail(err, ST_E_JSON, -1, "unencodable payload"); }
  char sha[65]; st_sha256_hex(b.data, b.len, sha);
  st_json_set(ev, "event_sha256", st_json_string(sha));
  st_buf line; st_buf_init(&line); st_json_write(&line, ev, ST_JSON_RFC8785); st_buf_put(&line, "\n", 1);
  ssize_t w = line.failed ? -1 : write(fd, line.data, line.len);
  int synced = w == (ssize_t)line.len && fsync(fd) == 0;
  st_buf_free(&b); st_buf_free(&line); st_json_free(ev); close(fd);
  if (!synced) return st_fail(err, ST_E_IO, -1, "journal write failed");
  if (out_sha) memcpy(out_sha, sha, 65);
  return ST_E_OK;
}
