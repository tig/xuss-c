#include "xuss/protocol.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect_true(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL %s\n", msg);
    fails++;
  }
}

int main(void) {
  xuss_proto_t p;
  xuss_proto_init(&p);
  xuss_proto_handle(&p, "identity");
  expect_true(p.n_resp == 1, "identity resp");
  expect_true(strstr(p.last_resp[0], "fw_name=XUSSC") != NULL, "identity name");

  xuss_proto_handle(&p, "rpm 800");
  expect_true(p.active_rpm == 800, "rpm live");
  xuss_proto_handle(&p, "repl");
  expect_true(p.exit_repl == 1, "repl door");

  if (fails) {
    return 1;
  }
  printf("ok protocol\n");
  return 0;
}
