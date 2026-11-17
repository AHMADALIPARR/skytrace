// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_EVIDENCE_H
#define SKYTRACE_EVIDENCE_H
/* Immutable content-addressed evidence store, case records, and the append-only,
 * hash-chained provenance journal. Behaviour of the case/ingest/verify paths matches the
 * original bin/skytrace.mjs contract (see tests/native/test_evidence.c). */
#include "skytrace/core.h"

/* 'ST-' + first 16 hex of SHA-256(source_id || 0x00 || source_record_id), uppercased. */
void st_stable_case_id(const char *source_id, size_t sn, const char *record_id, size_t rn, char out[20]);

/* Validates a case object exactly as validateCase() in bin/skytrace.mjs; on failure the
 * error message is the same text the JS CLI printed ("Missing claims", "Case ID mismatch", ...). */
st_code st_validate_case(const st_json *c, st_error *err);
/* RFC 3339 / ISO-8601 date or date-time (the subset accepted by ECMAScript Date.parse's ISO form). */
int st_parse_iso8601(const char *s, size_t n);

/* Evidence store rooted at <root>/data/raw. Objects are named <sha256>.<ext> and are never
 * overwritten: an existing object must already hash to the same digest or E_DIGEST is raised. */
typedef struct { char root[4096]; } st_store;
st_code st_store_open(st_store *s, const char *root, st_error *err);
st_code st_store_put_file(const st_store *s, const char *src_path, const char *ext,
                          char sha_hex[65], uint64_t *bytes, char *relative_path, size_t rp_cap, st_error *err);
/* Re-hashes <root>/<relative_path>; checks digest and byte count. */
st_code st_store_verify(const st_store *s, const char *relative_path, const char *sha_hex, uint64_t bytes, st_error *err);
st_code st_sha256_file(const char *path, char sha_hex[65], uint64_t *bytes, st_error *err);

/* Journal: <root>/data/manifests/events.ndjson. One RFC 8785 canonical object per line:
 *   {"actor","event_sha256","kind","payload","prev_event_sha256","seq","timestamp"}
 * event_sha256 = SHA-256 of the canonical object without event_sha256. Appends take an
 * exclusive fcntl lock, re-verify the tail, write with O_APPEND and fsync. */
st_code st_journal_append(const char *root, const char *actor, const char *kind, const st_json *payload,
                          const char *timestamp, char event_sha[65], st_error *err);
/* Verifies the full chain; returns number of events in *count. Missing journal = 0 events. */
st_code st_journal_verify(const char *root, size_t *count, char last_sha[65], st_error *err);

void st_now_iso8601_ms(char out[64]); /* e.g. 2026-11-17T18:39:00.123Z, as Date.toISOString() */

#endif
