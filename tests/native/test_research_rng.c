// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
/* Bit-exact agreement of src/core/rng.c with NumPy's PCG64 Generator and CPython's MT19937, against
 * draws captured from both (fixtures/reference/rng_streams.json), and regeneration of the release 2.2
 * physical fixture from its seeds (fixtures/reference/physical_fixture.seeds0-11.json). */
#include "skytrace/research.h"
#include "check.h"
#include <float.h>

static const st_json *G(const st_json *o, const char *k) { return st_json_get(o, k); }

int main(int argc, char **argv) {
  const char *root = argc > 1 ? argv[1] : ".";
  char path[4096]; st_error e;
  snprintf(path, sizeof path, "%s/fixtures/reference/rng_streams.json", root);
  st_json *fx = st_json_parse_file(path, &e);
  CHECK(fx != NULL);
  if (!fx) return finish("research_rng");
  const st_json *np = G(fx, "numpy"), *py = G(fx, "cpython");
  CHECK(np && np->count == 6 && py && py->count == 2);
  size_t exact = 0, total = 0;
#define EQ(a, b) do { total++; if ((a) == (b)) exact++; else CHECK((a) == (b)); } while (0)
  for (size_t c = 0; np && c < np->count; ++c) {
    const st_json *d = np->items[c]; st_np_rng g; st_np_seed(&g, (uint64_t)G(d, "seed")->ival);
    for (size_t i = 0; i < 5; ++i) EQ(st_np_random(&g), G(d, "random")->items[i]->num);
    for (size_t i = 0; i < 5; ++i) EQ(st_np_standard_normal(&g), G(d, "standard_normal")->items[i]->num);
    for (size_t i = 0; i < 3; ++i) EQ(st_np_uniform(&g, -.17, .17), G(d, "uniform_m017_017")->items[i]->num);
    for (size_t i = 0; i < 4; ++i) EQ(st_np_normal(&g, 0, .035), G(d, "normal_0_0035")->items[i]->num);
    size_t perm[20]; st_np_permutation(&g, 20, perm);
    for (size_t i = 0; i < 20; ++i) EQ((double)perm[i], G(d, "permutation_20")->items[i]->num);
    double z = 0, mx = 0;
    for (int i = 0; i < 20000; ++i) { z = st_np_standard_normal(&g); if (fabs(z) > mx) mx = fabs(z); }
    EQ(z, G(d, "normal_20000_last")->num); EQ(mx, G(d, "normal_20000_maxabs")->num);
    EQ(st_np_random(&g), G(d, "after_random")->num);
  }
  for (size_t c = 0; py && c < py->count; ++c) {
    const st_json *d = py->items[c]; st_py_rng r; st_py_seed(&r, (uint64_t)G(d, "seed")->ival);
    for (size_t i = 0; i < 5; ++i) EQ(st_py_random(&r), G(d, "random")->items[i]->num);
    for (size_t i = 0; i < 5; ++i) EQ(st_py_gauss(&r, 0, .002), G(d, "gauss_0_0002")->items[i]->num);
    for (size_t i = 0; i < 3; ++i) EQ(st_py_uniform(&r, 0.012, 0.035), G(d, "uniform_0012_0035")->items[i]->num);
    int l[20]; for (int i = 0; i < 20; ++i) l[i] = i;
    st_py_shuffle(&r, l, 20, sizeof *l);
    for (size_t i = 0; i < 20; ++i) EQ((double)l[i], G(d, "shuffle_20")->items[i]->num);
    EQ(st_py_random(&r), G(d, "after_random")->num);
  }
  st_json_free(fx);

  /* release 2.2 physical fixture, regenerated from the seed, equals the frozen capture bit for bit */
  snprintf(path, sizeof path, "%s/fixtures/reference/physical_fixture.seeds0-11.json", root);
  st_json *pf = st_json_parse_file(path, &e);
  size_t rows_exact = 0, rows_total = 0; double rows_maxabs = 0, rows_maxrel = 0;
  CHECK(pf != NULL);
  const st_json *cases = pf ? G(pf, "cases") : NULL;
  for (size_t c = 0; cases && c < cases->count; ++c) {
    const st_json *cs = cases->items[c]; st_r22_fixture f;
    st_r22_physical_fixture((uint64_t)G(cs, "seed")->ival, &f);
    for (int i = 0; i < 5; ++i) EQ(f.truth[i], G(cs, "truth")->items[i]->num);
    for (int i = 0; i < 3; ++i) EQ(f.offsets[i], G(cs, "offsets")->items[i]->num);
    const st_json *rows = G(cs, "rows");
    CHECK(rows->count == 60);
    for (size_t i = 0; i < rows->count && i < 60; ++i) for (int k = 0; k < 4; ++k) {
      double a = f.rows[i][k], b = rows->items[i]->items[k]->num, dd = fabs(a - b), rr = dd / (fabs(b) > 0 ? fabs(b) : 1);
      rows_total++; if (dd == 0) rows_exact++;
      if (dd > rows_maxabs) rows_maxabs = dd;
      if (rr > rows_maxrel) rows_maxrel = rr;
      CHECK(rr <= 4 * DBL_EPSILON); /* bearing/range go through atan2/hypot: libm vs NumPy may differ by ulps */
    }
  }
  st_json_free(pf);
  printf("bit-identical draws: %zu of %zu\n", exact, total);
  printf("physical fixture rows: %zu of %zu bit-identical, max abs %.3g, max rel %.3g\n", rows_exact, rows_total, rows_maxabs, rows_maxrel);
  return finish("research_rng");
}
