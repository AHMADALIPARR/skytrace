// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_ERROR_H
#define SKYTRACE_ERROR_H
/* Machine-readable failure reporting shared by every SKYTRACE C11 module.
 * Codes are stable identifiers; the printable names are part of the public contract
 * (spec/BUILD_SPEC.md section 2 plus the graph, numeric and bound codes below). */
#include <stddef.h>

typedef enum {
  ST_E_OK = 0,
  ST_E_SCHEMA, ST_E_SOURCE_POLICY, ST_E_SIZE_LIMIT, ST_E_DIGEST, ST_E_CONFLICT,
  ST_E_TIME, ST_E_CALIBRATION, ST_E_FRAME, ST_E_RANGE_UNIDENTIFIABLE, ST_E_INDEPENDENCE,
  ST_E_TOLERANCE, ST_E_PROOF, ST_E_PUBLICATION,
  ST_E_IO, ST_E_NOMEM, ST_E_JSON, ST_E_ARGUMENT,
  ST_E_CYCLE, ST_E_DUPLICATE, ST_E_DANGLING, ST_E_SELF_LOOP, ST_E_MULTIPLE_CONTAINMENT_PARENTS,
  ST_E_BOUND, ST_E_OVERFLOW, ST_E_NONFINITE, ST_E_SHAPE, ST_E_NOT_HERMITIAN,
  ST_E_NOT_POSITIVE, ST_E_NOT_CONVERGED, ST_E_SINGULAR, ST_E_JOURNAL,
  ST_E__COUNT
} st_code;

typedef struct {
  st_code code;
  long index;          /* offending element index, or -1 when not applicable */
  char message[256];   /* human-readable detail; never parsed by programs */
} st_error;

const char *st_code_name(st_code code);
/* Records the error (if err is non-NULL) and returns code, so callers can `return st_fail(...)`. */
st_code st_fail(st_error *err, st_code code, long index, const char *fmt, ...);
void st_error_clear(st_error *err);

#endif
