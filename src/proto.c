#include "gcu/proto.h"

#include "gcu/defaults.h"
#include "gcu/domain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gcu_proto_init(gcu_proto_t *p, void *user, gcu_proto_emit_fn emit,
                    gcu_proto_act_fn on_repl, gcu_proto_act_fn on_reboot,
                    gcu_proto_act_fn on_save) {
  memset(p, 0, sizeof *p);
  p->volume = GCU_DEFAULTS.volume_default;
  p->telemetry_hz = 0;
  p->user = user;
  p->emit = emit;
  p->on_repl = on_repl;
  p->on_reboot = on_reboot;
  p->on_save = on_save;
}

static void emitf(gcu_proto_t *p, const char *line) {
  if (p->emit) {
    p->emit(p->user, line);
  }
}

static int *param_slot(gcu_proto_t *p, const char *key) {
  if (strcmp(key, "mute") == 0) return &p->mute;
  if (strcmp(key, "volume") == 0) return &p->volume;
  if (strcmp(key, "telemetry_hz") == 0) return &p->telemetry_hz;
  return NULL;
}

static int param_ok(const char *key, long v) {
  if (strcmp(key, "mute") == 0) return v == 0 || v == 1;
  if (strcmp(key, "volume") == 0) return v >= 0 && v <= 127;
  return v >= 0 && v <= 1000; /* telemetry_hz */
}

static void dispatch(gcu_proto_t *p, char *line) {
  char out[GCU_PROTO_LINE_MAX];
  char *cmd = strtok(line, " ");
  if (!cmd || !*cmd) {
    return; /* empty line: ignore */
  }

  if (strcmp(cmd, "identity") == 0) {
    gcu_identity_line(out, (int)sizeof out);
    emitf(p, out);
    return;
  }
  if (strcmp(cmd, "repl") == 0) {
    emitf(p, "ok repl");
    if (p->on_repl) p->on_repl(p->user);
    return;
  }
  if (strcmp(cmd, "reboot") == 0) {
    emitf(p, "ok reboot");
    if (p->on_reboot) p->on_reboot(p->user);
    return;
  }
  if (strcmp(cmd, "get") == 0) {
    const char *key = strtok(NULL, " ");
    int *slot = key ? param_slot(p, key) : NULL;
    if (!slot) {
      emitf(p, "err unknown param");
      return;
    }
    snprintf(out, sizeof out, "%s=%d", key, *slot);
    emitf(p, out);
    return;
  }
  if (strcmp(cmd, "set") == 0) {
    const char *key = strtok(NULL, " ");
    const char *val = strtok(NULL, " ");
    int *slot = key ? param_slot(p, key) : NULL;
    char *end = NULL;
    long v = val ? strtol(val, &end, 10) : 0;
    if (!slot || !val || !end || *end != '\0' || !param_ok(key, v)) {
      emitf(p, "err bad set");
      return;
    }
    *slot = (int)v;
    snprintf(out, sizeof out, "%s=%d", key, *slot);
    emitf(p, out);
    return;
  }
  if (strcmp(cmd, "save") == 0) {
    if (p->on_save) {
      p->on_save(p->user);
      emitf(p, "ok save");
    } else {
      emitf(p, "err no persistence"); /* never claim a save that no-ops */
    }
    return;
  }
  if (strcmp(cmd, "defaults") == 0) {
    /* mute is exempt (spec §7.2 allows an exempt list — logged on #4). */
    p->volume = GCU_DEFAULTS.volume_default;
    p->telemetry_hz = 0;
    emitf(p, "ok defaults");
    return;
  }
  emitf(p, "err unknown command");
}

void gcu_proto_feed(gcu_proto_t *p, const char *bytes, int n) {
  for (int i = 0; i < n; i++) {
    char c = bytes[i];
    if (c == '\n' || c == '\r') {
      if (p->overflow) {
        p->overflow = 0;
        p->len = 0;
        emitf(p, "err line too long");
        continue;
      }
      if (p->len > 0) {
        p->buf[p->len] = '\0';
        p->len = 0;
        dispatch(p, p->buf);
      }
      continue;
    }
    if (p->overflow) {
      continue; /* discard until newline (fail closed) */
    }
    if (p->len >= GCU_PROTO_LINE_MAX - 1) {
      p->overflow = 1;
      continue;
    }
    p->buf[p->len++] = c;
  }
}
