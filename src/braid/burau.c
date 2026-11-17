/* Reduced Burau representation of B_3 over Z[t, t^-1] (faithful for n = 3):
 *   s1 -> [[-t, 1], [0, 1]]        s1^-1 -> [[-1/t, 1/t], [0, 1]]
 *   s2 -> [[1, 0], [t, -t]]        s2^-1 -> [[1, 0], [1, -1/t]]
 * Laurent polynomials are dense int64 arrays over degrees [-D, D], D = word length bound. */
#include "skytrace/braid.h"
#include <stdlib.h>
#include <string.h>

#define D ST_BRAID_MAX_WORD
#define W (2 * D + 1)
typedef struct { int64_t c[4][W]; } bmat;  /* entries 00, 01, 10, 11 */

static int add_ok(int64_t a, int64_t b, int64_t *r) {
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
  *r = a + b; return 1;
}
/* out += k * t^s * p ; k in {-1, 1} */
static int axpy(int64_t *out, const int64_t *p, int k, int s) {
  for (int d = 0; d < W; ++d) {
    if (!p[d]) continue;
    int e = d + s;
    if (e < 0 || e >= W) return 0;
    int64_t v = k > 0 ? p[d] : -p[d];
    if (k < 0 && p[d] == INT64_MIN) return 0;
    if (!add_ok(out[e], v, &out[e])) return 0;
  }
  return 1;
}
/* m <- m * g, where g entries are monomial sums: g[i][j] = sum of (coef, shift) terms. */
typedef struct { int n; int k[2]; int s[2]; } term;
static int right_mul(bmat *m, const term g[4], bmat *tmp) {
  memset(tmp, 0, sizeof *tmp);
  for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) for (int l = 0; l < 2; ++l) {
    const term *t = &g[l*2+j];
    for (int q = 0; q < t->n; ++q) if (!axpy(tmp->c[i*2+j], m->c[i*2+l], t->k[q], t->s[q])) return 0;
  }
  memcpy(m, tmp, sizeof *m); return 1;
}
static const term G[4][4] = {
  /* s1 */   {{1,{-1,0},{1,0}}, {1,{1,0},{0,0}}, {0,{0,0},{0,0}}, {1,{1,0},{0,0}}},
  /* s2 */   {{1,{1,0},{0,0}}, {0,{0,0},{0,0}}, {1,{1,0},{1,0}}, {1,{-1,0},{1,0}}},
  /* s1^-1 */{{1,{-1,0},{-1,0}}, {1,{1,0},{-1,0}}, {0,{0,0},{0,0}}, {1,{1,0},{0,0}}},
  /* s2^-1 */{{1,{1,0},{0,0}}, {0,{0,0},{0,0}}, {1,{1,0},{0,0}}, {1,{-1,0},{-1,0}}},
};

static st_code burau(const int *w, size_t n, bmat *m, bmat *tmp, st_error *err) {
  if (n > D) return st_fail(err, ST_E_BOUND, -1, "braid word longer than %d", D);
  memset(m, 0, sizeof *m); m->c[0][D] = 1; m->c[3][D] = 1;
  for (size_t i = 0; i < n; ++i) {
    int g = w[i] == 1 ? 0 : w[i] == 2 ? 1 : w[i] == -1 ? 2 : w[i] == -2 ? 3 : -1;
    if (g < 0) return st_fail(err, ST_E_ARGUMENT, (long)i, "braid generator must be +1,+2,-1,-2");
    if (!right_mul(m, G[g], tmp)) return st_fail(err, ST_E_OVERFLOW, (long)i, "Burau coefficient overflow");
  }
  return ST_E_OK;
}

st_code st_braid_equivalent(const int *w1, size_t n1, const int *w2, size_t n2, int *equal, st_error *err) {
  bmat *a = malloc(sizeof *a), *b = malloc(sizeof *b), *tmp = malloc(sizeof *tmp);
  if (!a || !b || !tmp) { free(a); free(b); free(tmp); return st_fail(err, ST_E_NOMEM, -1, "out of memory"); }
  st_code rc = burau(w1, n1, a, tmp, err);
  if (!rc) rc = burau(w2, n2, b, tmp, err);
  if (!rc) *equal = memcmp(a, b, sizeof *a) == 0;
  free(a); free(b); free(tmp); return rc;
}
