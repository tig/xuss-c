#include "gcu/protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  gcu_app_t app;
  char out[80];
  gcu_app_init(&app);

  /* Required verbs. */
  CHECK(gcu_protocol_handle(&app, "identity", out, sizeof out) ==
        GCU_PROTO_IDENTITY);
  CHECK(strstr(out, "fw_name=") && strstr(out, "fw_version="));
  CHECK(gcu_protocol_handle(&app, "repl", out, sizeof out) == GCU_PROTO_REPL);
  CHECK(gcu_protocol_handle(&app, "reboot", out, sizeof out) ==
        GCU_PROTO_REBOOT);

  /* Shorthand get/set. */
  CHECK(gcu_protocol_handle(&app, "mute 1", out, sizeof out) == GCU_PROTO_OK);
  CHECK(app.cfg.mute == 1);
  CHECK(gcu_protocol_handle(&app, "mute", out, sizeof out) == GCU_PROTO_OK);
  CHECK(strcmp(out, "mute=1") == 0);

  /* Generic get/set with range checks. */
  CHECK(gcu_protocol_handle(&app, "set volume 8", out, sizeof out) ==
        GCU_PROTO_OK);
  CHECK(app.cfg.volume == 8);
  CHECK(gcu_protocol_handle(&app, "get volume", out, sizeof out) ==
        GCU_PROTO_OK);
  CHECK(strcmp(out, "volume=8") == 0);
  CHECK(gcu_protocol_handle(&app, "set volume 99", out, sizeof out) ==
        GCU_PROTO_ERROR); /* out of range, fail closed */
  CHECK(app.cfg.volume == 8);
  CHECK(gcu_protocol_handle(&app, "mute 2", out, sizeof out) ==
        GCU_PROTO_ERROR);

  /* defaults resets volume/telemetry but keeps mute (exempt). */
  gcu_protocol_handle(&app, "telemetry_hz 5", out, sizeof out);
  CHECK(app.cfg.telemetry_hz == 5);
  CHECK(gcu_protocol_handle(&app, "defaults", out, sizeof out) ==
        GCU_PROTO_OK);
  CHECK(app.cfg.telemetry_hz == 0);
  CHECK(app.cfg.mute == 1); /* preserved */

  /* Malformed / bounds fail closed. */
  CHECK(gcu_protocol_handle(&app, "", out, sizeof out) == GCU_PROTO_ERROR);
  CHECK(gcu_protocol_handle(&app, "nope", out, sizeof out) == GCU_PROTO_ERROR);
  CHECK(gcu_protocol_handle(&app, "identity now", out, sizeof out) ==
        GCU_PROTO_ERROR);
  CHECK(gcu_protocol_handle(&app, "set volume x", out, sizeof out) ==
        GCU_PROTO_ERROR);
  char big[GCU_PROTO_MAX + 10];
  memset(big, 'a', sizeof big);
  big[sizeof big - 1] = '\0';
  CHECK(gcu_protocol_handle(&app, big, out, sizeof out) == GCU_PROTO_ERROR);

  printf("OK protocol\n");
  return 0;
}
