/* Protocol parser against spec.md §7: allow-list, bounded, fail-closed. */
#include "gcu/defaults.h"
#include "gcu/proto.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++;                                                  \
    }                                                           \
  } while (0)

static char last[GCU_PROTO_LINE_MAX];
static int repl_calls, reboot_calls;

static void emit(void *user, const char *line) {
  (void)user;
  snprintf(last, sizeof last, "%s", line);
}
static void on_repl(void *user) {
  (void)user;
  repl_calls++;
}
static void on_reboot(void *user) {
  (void)user;
  reboot_calls++;
}

static void feed(gcu_proto_t *p, const char *s) {
  gcu_proto_feed(p, s, (int)strlen(s));
}

int main(void) {
  gcu_proto_t p;
  gcu_proto_init(&p, NULL, emit, on_repl, on_reboot, NULL);

  /* identity */
  feed(&p, "identity\n");
  CHECK(strstr(last, "fw_name=XUSSC") != NULL);
  CHECK(strstr(last, "fw_version=") != NULL);

  /* unknown command fails closed with a short error */
  feed(&p, "rpm 4000\n");
  CHECK(strncmp(last, "err", 3) == 0);

  /* CRLF and split feeds */
  feed(&p, "iden");
  feed(&p, "tity\r\n");
  CHECK(strstr(last, "fw_name=XUSSC") != NULL);

  /* get/set with validation */
  feed(&p, "get mute\n");
  CHECK(strcmp(last, "mute=0") == 0);
  feed(&p, "set mute 1\n");
  CHECK(strcmp(last, "mute=1") == 0);
  CHECK(p.mute == 1);
  feed(&p, "set mute 7\n");
  CHECK(strncmp(last, "err", 3) == 0);
  CHECK(p.mute == 1);
  feed(&p, "set volume abc\n");
  CHECK(strncmp(last, "err", 3) == 0);
  feed(&p, "get bogus\n");
  CHECK(strncmp(last, "err", 3) == 0);

  /* defaults resets volume/telemetry, keeps mute (exempt list) */
  feed(&p, "set volume 99\n");
  CHECK(p.volume == 99);
  feed(&p, "defaults\n");
  CHECK(p.volume == GCU_DEFAULTS.volume_default);
  CHECK(p.mute == 1);

  /* escape hatch + reboot callbacks */
  feed(&p, "repl\n");
  CHECK(repl_calls == 1);
  feed(&p, "reboot\n");
  CHECK(reboot_calls == 1);

  /* overlong line: discard until newline, then recover */
  for (int i = 0; i < 300; i++) {
    gcu_proto_feed(&p, "x", 1);
  }
  feed(&p, "\n");
  CHECK(strcmp(last, "err line too long") == 0);
  feed(&p, "identity\n");
  CHECK(strstr(last, "fw_name=XUSSC") != NULL);

  /* Ctrl-C is data, not an interrupt */
  gcu_proto_feed(&p, "\x03\n", 2);
  CHECK(strncmp(last, "err", 3) == 0);

  if (fails) {
    return 1;
  }
  printf("OK proto\n");
  return 0;
}
