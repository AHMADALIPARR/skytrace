#include "skytrace/error.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const names[ST_E__COUNT] = {
  "OK", "E_SCHEMA", "E_SOURCE_POLICY", "E_SIZE_LIMIT", "E_DIGEST", "E_CONFLICT",
  "E_TIME", "E_CALIBRATION", "E_FRAME", "E_RANGE_UNIDENTIFIABLE", "E_INDEPENDENCE",
  "E_TOLERANCE", "E_PROOF", "E_PUBLICATION",
  "E_IO", "E_NOMEM", "E_JSON", "E_ARGUMENT",
  "E_CYCLE", "E_DUPLICATE", "E_DANGLING", "E_SELF_LOOP", "E_MULTIPLE_CONTAINMENT_PARENTS",
  "E_BOUND", "E_OVERFLOW", "E_NONFINITE", "E_SHAPE", "E_NOT_HERMITIAN",
  "E_NOT_POSITIVE", "E_NOT_CONVERGED", "E_SINGULAR", "E_JOURNAL"
};

const char *st_code_name(st_code code) {
  if ((int)code < 0 || code >= ST_E__COUNT) return "E_UNKNOWN_CODE";
  return names[code];
}

st_code st_fail(st_error *err, st_code code, long index, const char *fmt, ...) {
  if (err) {
    err->code = code;
    err->index = index;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof err->message, fmt, ap);
    va_end(ap);
  }
  return code;
}

void st_error_clear(st_error *err) {
  if (err) { err->code = ST_E_OK; err->index = -1; err->message[0] = '\0'; }
}
