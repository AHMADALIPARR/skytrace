// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#define _POSIX_C_SOURCE 200809L
#include "skytrace/evidence.h"
#include "check.h"
#include <unistd.h>

static st_json *parse(const char *s) { st_error e; return st_json_parse(s, strlen(s), &e); }
static const char *validate_msg(const char *json) {
  static char msg[256]; st_error e; st_error_clear(&e); st_json *c = parse(json);
  st_code rc = st_validate_case(c, &e); st_json_free(c);
  snprintf(msg, sizeof msg, "%s", rc ? e.message : "OK"); return msg;
}

int main(void) {
  char a[20], b[20], c2[20];
  /* tests/core.test.mjs: deterministic ID stable and namespace-separated */
  st_stable_case_id("aaro", 4, "PR-018", 6, a); st_stable_case_id("aaro", 4, "PR-018", 6, b); st_stable_case_id("pursue", 6, "PR-018", 6, c2);
  CHECK_STR(a, b); CHECK(strcmp(a, c2) != 0);
  CHECK(strncmp(a, "ST-", 3) == 0 && strlen(a) == 19);
  /* Separator prevents concatenation collisions: ("ab","c") != ("a","bc") */
  st_stable_case_id("ab", 2, "c", 1, b); st_stable_case_id("a", 1, "bc", 2, c2); CHECK(strcmp(b, c2) != 0);
  /* Value from the original Node implementation for ('aaro','PR-018'). */
  CHECK_STR(a, "ST-B7AA1750CD772D07");
  /* sha256 known test vector */
  char hex[65]; st_sha256_hex("abc", 3, hex); CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  /* valid empty-case schema accepted */
  char good[512];
  snprintf(good, sizeof good, "{\"case_id\":\"%s\",\"source_id\":\"aaro\",\"source_record_id\":\"PR-018\",\"observed_at\":null,"
           "\"time_precision\":\"unknown\",\"classification\":\"unassessed\",\"evidence\":[],\"claims\":[]}", a);
  CHECK_STR(validate_msg(good), "OK");
  /* reject corrupt ID */
  CHECK_STR(validate_msg("{\"case_id\":\"wrong\",\"source_id\":\"aaro\",\"source_record_id\":\"PR-018\",\"observed_at\":null,"
           "\"time_precision\":\"unknown\",\"classification\":\"unassessed\",\"evidence\":[],\"claims\":[]}"), "Case ID mismatch");
  /* reject evidence directory traversal */
  char trav[700];
  snprintf(trav, sizeof trav, "{\"case_id\":\"%s\",\"source_id\":\"aaro\",\"source_record_id\":\"PR-018\",\"observed_at\":null,"
           "\"time_precision\":\"unknown\",\"classification\":\"unassessed\",\"evidence\":[{\"sha256\":\"%064d\",\"relative_path\":\"../../private\"}],\"claims\":[]}", a, 0);
  CHECK_STR(validate_msg(trav), "Bad evidence path");
  /* remaining validateCase branches, in JS order */
  CHECK_STR(validate_msg("5"), "Case must be an object");
  CHECK_STR(validate_msg("null"), "Case must be an object");
  CHECK_STR(validate_msg("[]"), "Missing case_id");
  CHECK_STR(validate_msg("{\"case_id\":1}"), "Missing source_id");
  char t[800];
  const char *fmt = "{\"case_id\":\"%s\",\"source_id\":\"aaro\",\"source_record_id\":\"PR-018\",\"observed_at\":%s,"
                    "\"time_precision\":\"%s\",\"classification\":\"%s\",\"evidence\":%s,\"claims\":[]}";
  snprintf(t, sizeof t, fmt, a, "\"not a date\"", "unknown", "unassessed", "[]"); CHECK_STR(validate_msg(t), "Bad observed_at");
  snprintf(t, sizeof t, fmt, a, "\"2026-02-30\"", "unknown", "unassessed", "[]"); CHECK_STR(validate_msg(t), "Bad observed_at");
  snprintf(t, sizeof t, fmt, a, "\"2026-11-17T18:39:00.123Z\"", "exact", "identified", "[]"); CHECK_STR(validate_msg(t), "OK");
  snprintf(t, sizeof t, fmt, a, "\"2026-11-17T10:39-08:00\"", "minute", "identified", "[]"); CHECK_STR(validate_msg(t), "OK");
  snprintf(t, sizeof t, fmt, a, "null", "second", "unassessed", "[]"); CHECK_STR(validate_msg(t), "Bad precision");
  snprintf(t, sizeof t, fmt, a, "null", "unknown", "alien", "[]"); CHECK_STR(validate_msg(t), "Bad classification");
  snprintf(t, sizeof t, fmt, a, "null", "unknown", "unassessed", "{}"); CHECK_STR(validate_msg(t), "Evidence/claims must be arrays");
  snprintf(t, sizeof t, fmt, a, "null", "unknown", "unassessed", "[{\"sha256\":\"ABC\"}]"); CHECK_STR(validate_msg(t), "Bad evidence hash");
  char ev[300];
  snprintf(ev, sizeof ev, "[{\"sha256\":\"%064d\",\"relative_path\":\"data/raw/%064d.MP4\"}]", 0, 0);
  snprintf(t, sizeof t, fmt, a, "null", "unknown", "unassessed", ev); CHECK_STR(validate_msg(t), "Bad evidence path");
  snprintf(ev, sizeof ev, "[{\"sha256\":\"%064d\",\"relative_path\":\"data/raw/%064d.mp4\"}]", 0, 0);
  snprintf(t, sizeof t, fmt, a, "null", "unknown", "unassessed", ev); CHECK_STR(validate_msg(t), "OK");
  CHECK(st_parse_iso8601("2024-02-29", 10)); CHECK(!st_parse_iso8601("2023-02-29", 10)); CHECK(st_parse_iso8601("2026", 4));
  CHECK(!st_parse_iso8601("2026-13", 7)); CHECK(!st_parse_iso8601("2026-01-01T25:00Z", 17));

  /* Evidence store + journal in a scratch root */
  char root[] = "/tmp/skytrace-evidence-XXXXXX"; CHECK(mkdtemp(root) != NULL);
  char src[4200]; snprintf(src, sizeof src, "%s/sample.txt", root);
  FILE *f = fopen(src, "wb"); fputs("synthetic evidence bytes\n", f); fclose(f);
  st_store s; st_error e; char rel[200]; uint64_t n;
  CHECK(st_store_open(&s, root, &e) == ST_E_OK);
  CHECK(st_store_put_file(&s, src, "txt", hex, &n, rel, sizeof rel, &e) == ST_E_OK);
  CHECK(n == 25);
  CHECK(st_store_verify(&s, rel, hex, n, &e) == ST_E_OK);
  CHECK(st_store_put_file(&s, src, "txt", hex, &n, rel, sizeof rel, &e) == ST_E_OK); /* idempotent */
  char obj[4400]; snprintf(obj, sizeof obj, "%s/%s", root, rel);
  f = fopen(obj, "ab"); fputc('X', f); fclose(f);
  CHECK(st_store_verify(&s, rel, hex, n, &e) == ST_E_DIGEST);
  CHECK(st_store_put_file(&s, src, "txt", hex, &n, rel, sizeof rel, &e) == ST_E_DIGEST); /* never overwrites */
  size_t cnt; char last[65];
  CHECK(st_journal_verify(root, &cnt, last, &e) == ST_E_OK && cnt == 0);
  st_json *p1 = parse("{\"k\":1}"), *p2 = parse("{\"k\":[\"two\",2.5]}");
  char e1[65], e2[65];
  CHECK(st_journal_append(root, "test", "first", p1, "2026-11-17T00:00:00.000Z", e1, &e) == ST_E_OK);
  CHECK(st_journal_append(root, "test", "second", p2, "2026-11-17T00:00:01.000Z", e2, &e) == ST_E_OK);
  CHECK(st_journal_verify(root, &cnt, last, &e) == ST_E_OK && cnt == 2);
  CHECK_STR(last, e2);
  char jp[4400]; snprintf(jp, sizeof jp, "%s/data/manifests/events.ndjson", root);
  char *text; size_t len; CHECK(st_read_file(jp, &text, &len, 1 << 20, &e) == ST_E_OK);
  char *pos = strstr(text, "\"first\""); if (pos) pos[2] = 'X';            /* tamper with event 0 */
  f = fopen(jp, "wb"); fwrite(text, 1, len, f); fclose(f);
  CHECK(st_journal_verify(root, &cnt, last, &e) == ST_E_JOURNAL);
  CHECK(st_journal_append(root, "test", "third", p1, "2026-11-17T00:00:02.000Z", e1, &e) == ST_E_JOURNAL); /* refuses to extend a broken chain */
  if (pos) pos[2] = 'i';
  f = fopen(jp, "wb"); fwrite(text, 1, len - 1, f); fclose(f);              /* torn final line */
  CHECK(st_journal_verify(root, &cnt, last, &e) == ST_E_JOURNAL);
  free(text); st_json_free(p1); st_json_free(p2);
  char now[64]; st_now_iso8601_ms(now); CHECK(strlen(now) == 24 && now[23] == 'Z' && st_parse_iso8601(now, 24));
  char cmd[4300]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", root); CHECK(system(cmd) == 0);
  return finish("test_evidence");
}
