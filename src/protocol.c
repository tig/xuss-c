#include "gcu/protocol.h"
#include "gcu/defaults.h"
#include "gcu/domain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void say(char *out, int out_len, const char *s) {
  if (out && out_len > 0) {
    snprintf(out, (size_t)out_len, "%s", s);
  }
}

/* Split "key value" (single space). Returns the value token or NULL. */
static int parse_int(const char *s, long *v) {
  char *end = NULL;
  long n;
  if (!s || !*s) {
    return 0;
  }
  n = strtol(s, &end, 10);
  if (end == s || (end && *end != '\0')) {
    return 0; /* trailing junk fails closed */
  }
  *v = n;
  return 1;
}

/* Apply a commissioning key=value. Returns 1 on success, 0 on bad key/range. */
static int set_param(gcu_app_t *app, const char *key, const char *valstr) {
  long v;
  if (!parse_int(valstr, &v)) {
    return 0;
  }
  if (strcmp(key, "mute") == 0) {
    if (v != 0 && v != 1) {
      return 0;
    }
    app->cfg.mute = (int)v;
    return 1;
  }
  if (strcmp(key, "volume") == 0) {
    if (v < 0 || v > 10) {
      return 0;
    }
    app->cfg.volume = (int)v;
    return 1;
  }
  if (strcmp(key, "telemetry_hz") == 0) {
    if (v < 0) {
      return 0;
    }
    app->cfg.telemetry_hz = (int)v;
    return 1;
  }
  return 0;
}

static int get_param(const gcu_app_t *app, const char *key, char *out,
                     int out_len) {
  if (strcmp(key, "mute") == 0) {
    snprintf(out, (size_t)out_len, "mute=%d", app->cfg.mute);
    return 1;
  }
  if (strcmp(key, "volume") == 0) {
    snprintf(out, (size_t)out_len, "volume=%d", app->cfg.volume);
    return 1;
  }
  if (strcmp(key, "telemetry_hz") == 0) {
    snprintf(out, (size_t)out_len, "telemetry_hz=%d", app->cfg.telemetry_hz);
    return 1;
  }
  return 0;
}

gcu_proto_result_t gcu_protocol_handle(gcu_app_t *app, const char *line,
                                       char *out, int out_len) {
  char buf[GCU_PROTO_MAX + 1];
  char *cmd, *a1, *a2, *save;
  size_t len;

  if (!app || !line) {
    say(out, out_len, "err");
    return GCU_PROTO_ERROR;
  }
  len = strlen(line);
  if (len == 0 || len > GCU_PROTO_MAX) {
    say(out, out_len, "err bounds"); /* fail closed on empty/overlong */
    return GCU_PROTO_ERROR;
  }
  memcpy(buf, line, len);
  buf[len] = '\0';

  cmd = strtok_r(buf, " ", &save);
  a1 = strtok_r(NULL, " ", &save);
  a2 = strtok_r(NULL, " ", &save);
  if (!cmd) {
    say(out, out_len, "err");
    return GCU_PROTO_ERROR;
  }

  /* Required commands (§7.1). */
  if (strcmp(cmd, "identity") == 0 && !a1) {
    gcu_identity_line(out, out_len);
    return GCU_PROTO_IDENTITY;
  }
  if (strcmp(cmd, "repl") == 0 && !a1) {
    say(out, out_len, "ok repl");
    return GCU_PROTO_REPL;
  }
  if (strcmp(cmd, "reboot") == 0 && !a1) {
    say(out, out_len, "ok reboot");
    return GCU_PROTO_REBOOT;
  }

  /* Optional commissioning (§7.2). */
  if (strcmp(cmd, "get") == 0 && a1 && !a2) {
    if (get_param(app, a1, out, out_len)) {
      return GCU_PROTO_OK;
    }
    say(out, out_len, "err key");
    return GCU_PROTO_ERROR;
  }
  if (strcmp(cmd, "set") == 0 && a1 && a2) {
    if (set_param(app, a1, a2)) {
      say(out, out_len, "ok");
      return GCU_PROTO_OK;
    }
    say(out, out_len, "err value");
    return GCU_PROTO_ERROR;
  }
  /* Convenience shorthands: "mute [0|1]", "volume [n]", "telemetry_hz [n]". */
  if ((strcmp(cmd, "mute") == 0 || strcmp(cmd, "volume") == 0 ||
       strcmp(cmd, "telemetry_hz") == 0)) {
    if (!a1) {
      if (get_param(app, cmd, out, out_len)) {
        return GCU_PROTO_OK;
      }
    } else if (!a2 && set_param(app, cmd, a1)) {
      say(out, out_len, "ok");
      return GCU_PROTO_OK;
    }
    say(out, out_len, "err value");
    return GCU_PROTO_ERROR;
  }
  if (strcmp(cmd, "save") == 0 && !a1) {
    say(out, out_len, "ok save"); /* L0 has no persistence layer yet */
    return GCU_PROTO_OK;
  }
  if (strcmp(cmd, "defaults") == 0 && !a1) {
    int keep_mute = app->cfg.mute; /* mute is exempt from defaults (§7.2) */
    app->cfg.volume = GCU_DEFAULTS.volume;
    app->cfg.telemetry_hz = 0;
    app->cfg.mute = keep_mute;
    say(out, out_len, "ok defaults");
    return GCU_PROTO_OK;
  }

  say(out, out_len, "err unknown");
  return GCU_PROTO_ERROR;
}
