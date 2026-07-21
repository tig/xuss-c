#include "xuss/edge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int fails;

static void expect_near(double got, double want, double eps, const char *msg) {
  if (fabs(got - want) > eps) {
    fprintf(stderr, "FAIL %s: got %f want %f\n", msg, got, want);
    fails++;
  }
}

static void expect_int(int got, int want, const char *msg) {
  if (got != want) {
    fprintf(stderr, "FAIL %s: got %d want %d\n", msg, got, want);
    fails++;
  }
}

int main(void) {
  expect_near(xuss_rpm_to_hz(750, 130), 1625.0, 1e-9, "750rpm");
  expect_near(xuss_rpm_to_hz(200, 130), 200.0 * 130.0 / 60.0, 1e-9, "200rpm");
  expect_near(xuss_rpm_to_hz(1600, 130), 1600.0 * 130.0 / 60.0, 1e-9, "1600rpm");

  size_t n = 0;
  const xuss_profile_step_t *st = xuss_profile_steps("crank_catch_idle", &n);
  if (!st || n < 1) {
    fprintf(stderr, "FAIL missing crank profile\n");
    return 1;
  }
  expect_int(st[0].rpm, 200, "crank first");
  expect_int(xuss_profile_rpm_at(st, n, 900), 433, "crank mid");

  if (fails) {
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }
  printf("ok edge\n");
  return 0;
}
