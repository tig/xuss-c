#include "xuss/config.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect_int(int got, int want, const char *msg) {
  if (got != want) {
    fprintf(stderr, "FAIL %s: got %d want %d\n", msg, got, want);
    fails++;
  }
}

int main(void) {
  xuss_config_t cfg;
  xuss_config_factory(&cfg);
  expect_int(cfg.ring_teeth, 130, "factory teeth");
  expect_int(xuss_config_set(&cfg, "mute", "1"), 0, "set mute");
  expect_int(xuss_config_set(&cfg, "rpm", "1600"), 0, "set rpm");
  expect_int(cfg.rpm, 1600, "rpm");
  expect_int(xuss_config_set(&cfg, "rpm", "9000"), -1, "rpm range");
  xuss_config_defaults(&cfg);
  expect_int(cfg.mute, 1, "mute survives");
  expect_int(cfg.rpm, 0, "rpm reset");

  if (fails) {
    return 1;
  }
  printf("ok config\n");
  return 0;
}
