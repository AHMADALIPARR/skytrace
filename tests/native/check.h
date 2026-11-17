#ifndef SKYTRACE_TEST_CHECK_H
#define SKYTRACE_TEST_CHECK_H
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int st_checks, st_failures;
#define CHECK(cond) do { st_checks++; if (!(cond)) { st_failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); st_checks++; if (!(fabs(_a - _b) <= (tol))) { st_failures++; \
  fprintf(stderr, "FAIL %s:%d: %s = %.17g, expected %.17g (tol %g)\n", __FILE__, __LINE__, #a, _a, _b, (double)(tol)); } } while (0)
#define CHECK_STR(a, b) do { const char *_a = (a), *_b = (b); st_checks++; if (!_a || strcmp(_a, _b) != 0) { st_failures++; \
  fprintf(stderr, "FAIL %s:%d: %s = \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #a, _a ? _a : "(null)", _b); } } while (0)
static int finish(const char *suite) {
  printf("%s: %d checks, %d failures\n", suite, st_checks, st_failures);
  return st_failures ? 1 : 0;
}
#endif
