#include "skytrace/core.h"

st_tri st_tri_and(st_tri a, st_tri b) {
  if (a == ST_KNOWN_FALSE || b == ST_KNOWN_FALSE) return ST_KNOWN_FALSE;
  if (a == ST_KNOWN_TRUE && b == ST_KNOWN_TRUE) return ST_KNOWN_TRUE;
  return ST_UNKNOWN;
}
st_tri st_tri_or(st_tri a, st_tri b) {
  if (a == ST_KNOWN_TRUE || b == ST_KNOWN_TRUE) return ST_KNOWN_TRUE;
  if (a == ST_KNOWN_FALSE && b == ST_KNOWN_FALSE) return ST_KNOWN_FALSE;
  return ST_UNKNOWN;
}
st_tri st_tri_not(st_tri a) {
  return a == ST_UNKNOWN ? ST_UNKNOWN : (a == ST_KNOWN_TRUE ? ST_KNOWN_FALSE : ST_KNOWN_TRUE);
}
const char *st_tri_name(st_tri t) {
  return t == ST_KNOWN_TRUE ? "known_true" : t == ST_KNOWN_FALSE ? "known_false" : "unknown";
}
/* Codes: 1 present/yes, 0 absent/no, -1 unknown. A case stays open unless both range telemetry
 * and corroboration are known present; resolution==1 closes it (kernel policy, unknown stays open). */
int32_t st_case_incomplete(int32_t range, int32_t corroborated) {
  return (range != 1 || corroborated != 1) ? 1 : 0;
}
int32_t st_case_active(int32_t range, int32_t corroborated, int32_t resolution) {
  if (resolution == 1) return 0;
  return st_case_incomplete(range, corroborated);
}
/* Publication policy from research/numerical/analyze.py: unknown resolution is never auto-admitted. */
int32_t st_case_active_strict(int32_t range, int32_t corroborated, int32_t resolution) {
  if (resolution != 0) return 0;
  return st_case_incomplete(range, corroborated);
}
