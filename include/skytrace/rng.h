// SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
// Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
#ifndef SKYTRACE_RNG_H
#define SKYTRACE_RNG_H
/* Bit-exact reimplementations of the two random streams the SKYTRACE research benchmarks were
 * frozen with, so the C11 ports regenerate the same synthetic data instead of shipping it:
 *
 *   st_np_*  NumPy Generator(PCG64) as built by numpy.random.default_rng(seed):
 *            SeedSequence(seed) -> generate_state(4, uint64) -> PCG64 (XSL-RR 128/64);
 *            random(), uniform(lo, hi), standard_normal (256-layer ziggurat), normal(loc, scale),
 *            permutation(n) (Fisher-Yates driven by random_interval).
 *   st_py_*  CPython random.Random(seed): MT19937 init_by_array, random(), uniform(a, b),
 *            gauss(mu, sigma) (cached pair), shuffle (getrandbits rejection sampling).
 *
 * Equality with the reference streams is checked by tests/native/test_research_rng.c against
 * draws captured from NumPy 2.5.5 / CPython 3.13 (fixtures/reference/rng_streams.json). */
#include <stddef.h>
#include <stdint.h>

typedef struct { uint64_t state_hi, state_lo, inc_hi, inc_lo; int has_uint32; uint32_t uinteger; } st_np_rng;
/* SeedSequence entropy = little-endian 32-bit words of a non-negative integer (0 -> [0]). */
void st_np_seed(st_np_rng *g, uint64_t seed);
uint64_t st_np_next64(st_np_rng *g);
uint32_t st_np_next32(st_np_rng *g);
double st_np_random(st_np_rng *g);                       /* [0, 1), 53 bits */
double st_np_uniform(st_np_rng *g, double lo, double hi); /* lo + (hi - lo) * random() */
double st_np_standard_normal(st_np_rng *g);
double st_np_normal(st_np_rng *g, double loc, double scale);
uint64_t st_np_interval(st_np_rng *g, uint64_t max);     /* uniform on [0, max] */
void st_np_permutation(st_np_rng *g, size_t n, size_t *out);

typedef struct { uint32_t mt[624]; int mti; int has_gauss; double gauss_next; } st_py_rng;
void st_py_seed(st_py_rng *r, uint64_t seed);
uint32_t st_py_genrand32(st_py_rng *r);
double st_py_random(st_py_rng *r);
double st_py_uniform(st_py_rng *r, double a, double b);
double st_py_gauss(st_py_rng *r, double mu, double sigma);
uint64_t st_py_randbelow(st_py_rng *r, uint64_t n);      /* n >= 1, n < 2^32 */
/* random.shuffle on an array of n elements of `size` bytes (size <= 256). */
void st_py_shuffle(st_py_rng *r, void *base, size_t n, size_t size);

#endif
