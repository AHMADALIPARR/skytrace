// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#include "skytrace/rng.h"
#include "rng_tables.h"
#include <math.h>
#include <string.h>

/* ---------------------------------------------------------------- 128-bit helpers */
static void mul64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo) {
  uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
  uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
  uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
  *lo = (p00 & 0xffffffffu) | (mid << 32);
  *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}
#define PCG_MUL_HI 2549297995355413924ULL
#define PCG_MUL_LO 4865540595714422341ULL
static void pcg_step(st_np_rng *g) {
  uint64_t hi, lo;
  mul64(g->state_lo, PCG_MUL_LO, &hi, &lo);
  hi += g->state_hi * PCG_MUL_LO + g->state_lo * PCG_MUL_HI;
  uint64_t nlo = lo + g->inc_lo;
  hi += g->inc_hi + (nlo < lo);
  g->state_lo = nlo; g->state_hi = hi;
}
static uint64_t rotr64(uint64_t v, unsigned r) { r &= 63u; return r ? (v >> r) | (v << (64u - r)) : v; }

/* ---------------------------------------------------------------- SeedSequence (pool size 4) */
#define SS_INIT_A 0x43b0d7e5u
#define SS_MULT_A 0x931e8875u
#define SS_INIT_B 0x8b51f9ddu
#define SS_MULT_B 0x58f38dedu
#define SS_MIX_L 0xca01f9ddu
#define SS_MIX_R 0x4973f715u
static uint32_t hashmix(uint32_t v, uint32_t *hc) { v ^= *hc; *hc *= SS_MULT_A; v *= *hc; v ^= v >> 16; return v; }
static uint32_t mix(uint32_t x, uint32_t y) { uint32_t r = SS_MIX_L * x - SS_MIX_R * y; r ^= r >> 16; return r; }

void st_np_seed(st_np_rng *g, uint64_t seed) {
  uint32_t ent[2]; size_t ne = 0;
  if (seed == 0) ent[ne++] = 0;
  while (seed) { ent[ne++] = (uint32_t)(seed & 0xffffffffu); seed >>= 32; }
  uint32_t pool[4], hc = SS_INIT_A;
  for (size_t i = 0; i < 4; ++i) pool[i] = hashmix(i < ne ? ent[i] : 0u, &hc);
  for (size_t s = 0; s < 4; ++s)
    for (size_t d = 0; d < 4; ++d)
      if (s != d) pool[d] = mix(pool[d], hashmix(pool[s], &hc));
  /* entropy longer than the pool is impossible here (ne <= 2) */
  uint32_t words[8], hb = SS_INIT_B;
  for (size_t i = 0; i < 8; ++i) {
    uint32_t v = pool[i % 4];
    v ^= hb; hb *= SS_MULT_B; v *= hb; v ^= v >> 16; words[i] = v;
  }
  uint64_t s0 = (uint64_t)words[0] | ((uint64_t)words[1] << 32), s1 = (uint64_t)words[2] | ((uint64_t)words[3] << 32);
  uint64_t i0 = (uint64_t)words[4] | ((uint64_t)words[5] << 32), i1 = (uint64_t)words[6] | ((uint64_t)words[7] << 32);
  /* pcg_setseq_128_srandom_r(initstate = s0:s1, initseq = i0:i1) */
  g->state_hi = 0; g->state_lo = 0;
  g->inc_hi = (i0 << 1) | (i1 >> 63); g->inc_lo = (i1 << 1) | 1u;
  pcg_step(g);
  uint64_t lo = g->state_lo + s1; g->state_hi += s0 + (lo < s1); g->state_lo = lo;
  pcg_step(g);
  g->has_uint32 = 0; g->uinteger = 0;
}

uint64_t st_np_next64(st_np_rng *g) { pcg_step(g); return rotr64(g->state_hi ^ g->state_lo, (unsigned)(g->state_hi >> 58)); }
uint32_t st_np_next32(st_np_rng *g) {
  if (g->has_uint32) { g->has_uint32 = 0; return g->uinteger; }
  uint64_t n = st_np_next64(g);
  g->has_uint32 = 1; g->uinteger = (uint32_t)(n >> 32);
  return (uint32_t)(n & 0xffffffffu);
}
double st_np_random(st_np_rng *g) { return (double)(st_np_next64(g) >> 11) * (1.0 / 9007199254740992.0); }
double st_np_uniform(st_np_rng *g, double lo, double hi) { return lo + (hi - lo) * st_np_random(g); }
double st_np_standard_normal(st_np_rng *g) {
  for (;;) {
    uint64_t r = st_np_next64(g);
    int idx = (int)(r & 0xff);
    r >>= 8;
    int sign = (int)(r & 1u);
    uint64_t rabs = (r >> 1) & 0x000fffffffffffffULL;
    double x = (double)rabs * st_np_wi[idx];
    if (sign) x = -x;
    if (rabs < st_np_ki[idx]) return x;
    if (idx == 0) {
      for (;;) {
        double xx = -ST_NP_ZIG_NOR_INV_R * log1p(-st_np_random(g));
        double yy = -log1p(-st_np_random(g));
        if (yy + yy > xx * xx) return ((rabs >> 8) & 1u) ? -(ST_NP_ZIG_NOR_R + xx) : ST_NP_ZIG_NOR_R + xx;
      }
    } else if ((st_np_fi[idx - 1] - st_np_fi[idx]) * st_np_random(g) + st_np_fi[idx] < exp(-0.5 * x * x)) {
      return x;
    }
  }
}
double st_np_normal(st_np_rng *g, double loc, double scale) { return loc + scale * st_np_standard_normal(g); }
uint64_t st_np_interval(st_np_rng *g, uint64_t max) {
  if (max == 0) return 0;
  uint64_t mask = max, v;
  mask |= mask >> 1; mask |= mask >> 2; mask |= mask >> 4; mask |= mask >> 8; mask |= mask >> 16; mask |= mask >> 32;
  if (max <= 0xffffffffULL) { while ((v = (st_np_next32(g) & mask)) > max) {} }
  else { while ((v = (st_np_next64(g) & mask)) > max) {} }
  return v;
}
void st_np_permutation(st_np_rng *g, size_t n, size_t *out) {
  for (size_t i = 0; i < n; ++i) out[i] = i;
  for (size_t i = n; i-- > 1;) {
    size_t j = (size_t)st_np_interval(g, (uint64_t)i);
    size_t t = out[j]; out[j] = out[i]; out[i] = t;
  }
}

/* ---------------------------------------------------------------- CPython MT19937 */
static void mt_init(st_py_rng *r, uint32_t s) {
  r->mt[0] = s;
  for (int i = 1; i < 624; ++i) r->mt[i] = 1812433253u * (r->mt[i-1] ^ (r->mt[i-1] >> 30)) + (uint32_t)i;
  r->mti = 624;
}
void st_py_seed(st_py_rng *r, uint64_t seed) {
  uint32_t key[2]; size_t klen = 0;
  if (seed == 0) key[klen++] = 0;
  while (seed) { key[klen++] = (uint32_t)(seed & 0xffffffffu); seed >>= 32; }
  mt_init(r, 19650218u);
  size_t i = 1, j = 0, k = 624 > klen ? 624 : klen;
  for (; k; --k) {
    r->mt[i] = (r->mt[i] ^ ((r->mt[i-1] ^ (r->mt[i-1] >> 30)) * 1664525u)) + key[j] + (uint32_t)j;
    ++i; ++j;
    if (i >= 624) { r->mt[0] = r->mt[623]; i = 1; }
    if (j >= klen) j = 0;
  }
  for (k = 623; k; --k) {
    r->mt[i] = (r->mt[i] ^ ((r->mt[i-1] ^ (r->mt[i-1] >> 30)) * 1566083941u)) - (uint32_t)i;
    ++i;
    if (i >= 624) { r->mt[0] = r->mt[623]; i = 1; }
  }
  r->mt[0] = 0x80000000u;
  r->has_gauss = 0; r->gauss_next = 0;
}
uint32_t st_py_genrand32(st_py_rng *r) {
  static const uint32_t mag01[2] = {0u, 0x9908b0dfu};
  uint32_t y;
  if (r->mti >= 624) {
    int kk;
    for (kk = 0; kk < 624 - 397; ++kk) { y = (r->mt[kk] & 0x80000000u) | (r->mt[kk+1] & 0x7fffffffu); r->mt[kk] = r->mt[kk+397] ^ (y >> 1) ^ mag01[y & 1u]; }
    for (; kk < 623; ++kk) { y = (r->mt[kk] & 0x80000000u) | (r->mt[kk+1] & 0x7fffffffu); r->mt[kk] = r->mt[kk+(397-624)] ^ (y >> 1) ^ mag01[y & 1u]; }
    y = (r->mt[623] & 0x80000000u) | (r->mt[0] & 0x7fffffffu);
    r->mt[623] = r->mt[396] ^ (y >> 1) ^ mag01[y & 1u];
    r->mti = 0;
  }
  y = r->mt[r->mti++];
  y ^= (y >> 11); y ^= (y << 7) & 0x9d2c5680u; y ^= (y << 15) & 0xefc60000u; y ^= (y >> 18);
  return y;
}
double st_py_random(st_py_rng *r) {
  uint32_t a = st_py_genrand32(r) >> 5, b = st_py_genrand32(r) >> 6;
  return ((double)a * 67108864.0 + (double)b) * (1.0 / 9007199254740992.0);
}
double st_py_uniform(st_py_rng *r, double a, double b) { return a + (b - a) * st_py_random(r); }
double st_py_gauss(st_py_rng *r, double mu, double sigma) {
  double z;
  if (r->has_gauss) { z = r->gauss_next; r->has_gauss = 0; }
  else {
    const double TWOPI = 2.0 * 3.141592653589793;
    double x2pi = st_py_random(r) * TWOPI;
    double g2rad = sqrt(-2.0 * log(1.0 - st_py_random(r)));
    z = cos(x2pi) * g2rad;
    r->gauss_next = sin(x2pi) * g2rad; r->has_gauss = 1;
  }
  return mu + z * sigma;
}
uint64_t st_py_randbelow(st_py_rng *r, uint64_t n) {
  unsigned k = 0; for (uint64_t m = n; m; m >>= 1) ++k;   /* n.bit_length() */
  uint64_t v;
  do { v = st_py_genrand32(r) >> (32u - k); } while (v >= n);
  return v;
}
void st_py_shuffle(st_py_rng *r, void *base, size_t n, size_t size) {
  unsigned char tmp[256], *b = base;
  for (size_t i = n; i-- > 1;) {
    size_t j = (size_t)st_py_randbelow(r, (uint64_t)i + 1);
    memcpy(tmp, b + i * size, size); memcpy(b + i * size, b + j * size, size); memcpy(b + j * size, tmp, size);
  }
}
