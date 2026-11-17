// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* End-to-end CLI test. Replays fixtures/reference/cli_sequence.txt against the native binary in a fresh
 * temporary root and compares the masked transcript byte-for-byte with cli_transcript.node.txt, which was
 * captured by running the original bin/skytrace.mjs under Node with the same sequence and masking. */
#define _POSIX_C_SOURCE 200809L
#include "skytrace/evidence.h"
#include "check.h"
#include <unistd.h>

static const char *BIN, *SRC;
static char ROOT[256];

static char *slurp(const char *path, size_t *n) {
  char *d = NULL; size_t len = 0; st_error e;
  if (st_read_file(path, &d, &len, 64u << 20, &e)) { d = malloc(1); d[0] = 0; len = 0; }
  if (n) *n = len;
  return d;
}
static void spit(const char *path, const char *s) { FILE *f = fopen(path, "wb"); if (f) { fputs(s, f); fclose(f); } }

/* Run the binary with SKYTRACE_ROOT set, cwd = ROOT; returns exit status. */
static int run(const char *args, const char *stdin_path, const char *out, const char *err) {
  char cmd[8192];
  snprintf(cmd, sizeof cmd, "cd '%s' && SKYTRACE_ROOT='%s' '%s' %s <'%s' >'%s' 2>'%s'", ROOT, ROOT, BIN, args,
           stdin_path ? stdin_path : "/dev/null", out, err);
  int s = system(cmd);
  return s == -1 ? -1 : (s >> 8) & 0xff;
}

static void replace_all(st_buf *b, const char *from, const char *to) {
  st_buf o; st_buf_init(&o); size_t fl = strlen(from);
  for (size_t i = 0; i < b->len;) {
    if (i + fl <= b->len && !memcmp(b->data + i, from, fl)) { st_buf_puts(&o, to); i += fl; }
    else st_buf_put(&o, b->data + i, 1), i++;
  }
  st_buf_free(b); *b = o;
}
static void mask_time(st_buf *b) {
  const char *key = "\"ingested_at\": \""; size_t kl = strlen(key);
  st_buf o; st_buf_init(&o);
  for (size_t i = 0; i < b->len;) {
    if (i + kl <= b->len && !memcmp(b->data + i, key, kl)) {
      st_buf_put(&o, key, kl); st_buf_puts(&o, "<TIME>\""); i += kl;
      while (i < b->len && b->data[i] != '"') i++;
      i++;
    } else st_buf_put(&o, b->data + i, 1), i++;
  }
  st_buf_free(b); *b = o;
}
static void append_prefixed(st_buf *t, const char *data, size_t n, const char *prefix) {
  size_t start = 0;
  for (size_t i = 0; i < n; ++i) if (data[i] == '\n') { st_buf_puts(t, prefix); st_buf_put(t, data + start, i - start + 1); start = i + 1; }
  if (start < n) { st_buf_puts(t, prefix); st_buf_put(t, data + start, n - start); st_buf_puts(t, "\n"); }
}

static void transcript_test(void) {
  char p[1024], out[512], err[512];
  snprintf(out, sizeof out, "%s/.out", ROOT); snprintf(err, sizeof err, "%s/.err", ROOT);
  snprintf(p, sizeof p, "%s/data", ROOT); st_error e; st_mkdir_p(p, &e);
  char *sources = slurp((snprintf(p, sizeof p, "%s/data/sources.json", SRC), p), NULL);
  snprintf(p, sizeof p, "%s/data/sources.json", ROOT); spit(p, sources); free(sources);
  snprintf(p, sizeof p, "%s/sample.MP4", ROOT); spit(p, "synthetic evidence bytes\n");
  snprintf(p, sizeof p, "%s/noext", ROOT); spit(p, "x\n");
  snprintf(p, sizeof p, "%s/file.toolongext", ROOT); spit(p, "y\n");
  char *seq = slurp((snprintf(p, sizeof p, "%s/fixtures/reference/cli_sequence.txt", SRC), p), NULL);
  st_buf t; st_buf_init(&t);
  for (char *line = strtok(seq, "\n"); line; line = strtok(NULL, "\n")) {
    st_buf_puts(&t, "$ "); st_buf_puts(&t, line); st_buf_puts(&t, "\n");
    if (!strncmp(line, "@cat ", 5)) {
      size_t n; snprintf(p, sizeof p, "%s/%s", ROOT, line + 5); char *d = slurp(p, &n); st_buf_put(&t, d, n); free(d);
    } else if (!strncmp(line, "@tamper ", 8)) {
      snprintf(p, sizeof p, "%s/%s", ROOT, line + 8); spit(p, "tampered\n");
    } else {
      int code = run(line, NULL, out, err); size_t n;
      char *o = slurp(out, &n); st_buf_put(&t, o, n); free(o);
      o = slurp(err, &n); append_prefixed(&t, o, n, "[stderr] "); free(o);
      char tail[32]; snprintf(tail, sizeof tail, "[exit %d]\n", code); st_buf_puts(&t, tail);
    }
  }
  free(seq);
  replace_all(&t, ROOT, "<ROOT>"); mask_time(&t);
  size_t rn; char *ref = slurp((snprintf(p, sizeof p, "%s/fixtures/reference/cli_transcript.node.txt", SRC), p), &rn);
  CHECK(t.len == rn && !memcmp(t.data, ref, rn));
  if (!(t.len == rn && !memcmp(t.data, ref, rn))) { snprintf(p, sizeof p, "%s/.transcript", ROOT); FILE *f = fopen(p, "wb"); fwrite(t.data, 1, t.len, f); fclose(f); fprintf(stderr, "transcript written to %s\n", p); }
  free(ref); st_buf_free(&t);
  /* The native CLI adds a hash-chained journal: one evidence_ingested event per successful ingest. */
  size_t count; char last[65];
  CHECK(st_journal_verify(ROOT, &count, last, &e) == ST_E_OK); CHECK(count == 3); CHECK(strlen(last) == 64);
  /* Tampering with the journal is detected by verify (after restoring the evidence file). */
  snprintf(p, sizeof p, "%s/data/raw/d7b3ec660dea7159af1e7835a9a488a6f19e3d6bf67de5bcafba1ed7d25315d4.mp4", ROOT);
  spit(p, "synthetic evidence bytes\n");
  CHECK(run("verify", NULL, out, err) == 0);
  snprintf(p, sizeof p, "%s/data/manifests/events.ndjson", ROOT);
  size_t jn; char *j = slurp(p, &jn); char *hit = strstr(j, "\"bytes\":25");
  CHECK(hit != NULL); if (hit) hit[9] = '6';
  FILE *f = fopen(p, "wb"); fwrite(j, 1, jn, f); fclose(f); free(j);
  CHECK(run("verify", NULL, out, err) == 1);
  /* Unknown command prints usage and fails. */
  CHECK(run("frobnicate", NULL, out, err) == 1);
  char *m = slurp(err, NULL); CHECK(strstr(m, "ERROR: Commands: sources") == m); free(m);
}

static void compare_json_file(const char *got_path, const char *ref_rel, const char *const *sections) {
  char p[1024]; st_error e; snprintf(p, sizeof p, "%s/%s", SRC, ref_rel);
  st_json *got = st_json_parse_file(got_path, &e), *ref = st_json_parse_file(p, &e);
  CHECK(got && ref);
  if (got && ref) {
    if (!sections) CHECK(st_json_equal(got, ref));
    else for (size_t i = 0; sections[i]; ++i) { CHECK(st_json_get(got, sections[i]) != NULL); CHECK(st_json_equal(st_json_get(got, sections[i]), st_json_get(ref, sections[i]))); }
  }
  st_json_free(got); st_json_free(ref);
}

static void reports_test(void) {
  char in[1024], out[512], err[512], args[1200];
  snprintf(out, sizeof out, "%s/.out", ROOT); snprintf(err, sizeof err, "%s/.err", ROOT);
  /* analyze: byte-identical to numerics/sensor_graph.py output on the same NDJSON */
  const char *pairs[2][2] = {{"fixtures/sensors/synthetic-graph.ndjson", "fixtures/reference/sensor_graph.synthetic-graph.out.json"},
                             {"fixtures/reference/sensor_graph.differential.ndjson", "fixtures/reference/sensor_graph.differential.out.json"}};
  for (int k = 0; k < 2; ++k) {
    snprintf(in, sizeof in, "%s/%s", SRC, pairs[k][0]);
    CHECK(run("analyze", in, out, err) == 0);
    size_t gn, rn; char *g = slurp(out, &gn); snprintf(in, sizeof in, "%s/%s", SRC, pairs[k][1]); char *r = slurp(in, &rn);
    CHECK(gn == rn && !memcmp(g, r, rn)); free(g); free(r);
  }
  /* records: graph, observational_data and ternary_closure equal research/numerical/analyze.py output */
  snprintf(args, sizeof args, "records '%s/fixtures/archives/synthetic-batch.json'", SRC);
  CHECK(run(args, NULL, out, err) == 0);
  const char *const secs[] = {"graph", "observational_data", "ternary_closure", NULL};
  compare_json_file(out, "fixtures/reference/numerical_analyze.synthetic-batch.out.json", secs);
  CHECK(run("records /nonexistent.json", NULL, out, err) == 1);
  /* braid-equiv uses exact Burau equivalence */
  CHECK(run("braid-equiv 1,2,1 2,1,2", NULL, out, err) == 0);
  char *o = slurp(out, NULL); CHECK(strstr(o, "true") != NULL); free(o);
  CHECK(run("braid-equiv 1,2 2,1", NULL, out, err) == 0);
  o = slurp(out, NULL); CHECK(strstr(o, "false") != NULL); free(o);
}

int main(int argc, char **argv) {
  if (argc != 3) { fprintf(stderr, "usage: test_cli <skytrace-binary> <source-dir>\n"); return 2; }
  BIN = argv[1]; SRC = argv[2];
  snprintf(ROOT, sizeof ROOT, "/tmp/skytrace-cli-XXXXXX");
  if (!mkdtemp(ROOT)) { perror("mkdtemp"); return 2; }
  transcript_test();
  reports_test();
  char cmd[512]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", ROOT);
  if (!st_failures && system(cmd) != 0) fprintf(stderr, "cleanup failed for %s\n", ROOT);
  return finish("test_cli");
}
