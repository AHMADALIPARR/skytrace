// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Structural comparison of a ported report against a committed Python results file. */
#include "skytrace/research.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int skipped(const st_cmp_policy *p, const char *key) {
  for (size_t i = 0; i < p->nskip; ++i) if (!strcmp(p->skip_keys[i], key)) return 1;
  return 0;
}
static void mismatch(st_cmp_report *r, const char *path, const char *what) {
  if (!r->mismatches++) snprintf(r->first_mismatch, sizeof r->first_mismatch, "%s: %s", path, what);
}
static void walk(const st_json *a, const st_json *e, const st_cmp_policy *p, st_cmp_report *r, const char *path) {
  char sub[600], msg[300];
  if (!a) { mismatch(r, path, "missing in port"); return; }
  switch (e->type) {
    case ST_JOBJ:
      if (a->type != ST_JOBJ) { mismatch(r, path, "type"); return; }
      for (size_t i = 0; i < e->count; ++i) {
        snprintf(sub, sizeof sub, "%s/%s", path, e->keys[i]);
        if (skipped(p, e->keys[i])) { r->skipped++; continue; }
        walk(st_json_get(a, e->keys[i]), e->items[i], p, r, sub);
      }
      return;
    case ST_JARR:
      if (a->type != ST_JARR || a->count != e->count) { mismatch(r, path, "array length"); return; }
      for (size_t i = 0; i < e->count; ++i) { snprintf(sub, sizeof sub, "%s[%zu]", path, i); walk(a->items[i], e->items[i], p, r, sub); }
      return;
    case ST_JNUM: {
      if (a->type != ST_JNUM) { mismatch(r, path, "type"); return; }
      r->numbers++;
      double d = fabs(a->num - e->num);
      if (a->num == e->num) { r->exact++; return; }
      if (e->is_int && a->is_int) { snprintf(msg, sizeof msg, "integer %.17g != %.17g", a->num, e->num); mismatch(r, path, msg); return; }
      if (d > r->max_abs_diff) { r->max_abs_diff = d; snprintf(r->worst_path, sizeof r->worst_path, "%s", path); }
      if (!(d <= p->atol + p->rtol * fabs(e->num))) { snprintf(msg, sizeof msg, "%.17g vs %.17g (diff %.3g)", a->num, e->num, d); mismatch(r, path, msg); }
      return;
    }
    case ST_JSTR:
      if (a->type != ST_JSTR || a->len != e->len || memcmp(a->str, e->str, e->len)) mismatch(r, path, "string differs");
      return;
    case ST_JBOOL:
      if (a->type != ST_JBOOL || a->boolean != e->boolean) mismatch(r, path, "boolean differs");
      return;
    case ST_JNULL:
      if (a->type != ST_JNULL) mismatch(r, path, "expected null");
      return;
  }
}
void st_json_compare(const st_json *actual, const st_json *expected, const st_cmp_policy *p, st_cmp_report *r) {
  memset(r, 0, sizeof *r);
  walk(actual, expected, p, r, "");
}
