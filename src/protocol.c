#include "xuss/protocol.h"

#include <stdio.h>
#include <string.h>

void xuss_proto_identity(char *out, int out_len) {
  if (!out || out_len <= 0) {
    return;
  }
  snprintf(out, (size_t)out_len, "fw_name=%s fw_version=%s", XUSS_FW_NAME, XUSS_FW_VERSION);
}

void xuss_proto_init(xuss_proto_t *p) {
  if (!p) {
    return;
  }
  memset(p, 0, sizeof(*p));
  xuss_config_factory(&p->cfg);
}

static void push(xuss_proto_t *p, const char *s) {
  if (!p || p->n_resp >= 8) {
    return;
  }
  strncpy(p->last_resp[p->n_resp], s, sizeof(p->last_resp[0]) - 1);
  p->n_resp++;
}

void xuss_proto_handle(xuss_proto_t *p, const char *line) {
  if (!p) {
    return;
  }
  p->n_resp = 0;
  if (!line || !line[0]) {
    return;
  }

  char buf[128];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;

  char *cmd = strtok(buf, " \t");
  char *a1 = strtok(NULL, " \t");
  char *a2 = strtok(NULL, " \t");
  if (!cmd) {
    return;
  }

  if (strcmp(cmd, "identity") == 0) {
    char id[64];
    xuss_proto_identity(id, sizeof(id));
    push(p, id);
    return;
  }
  if (strcmp(cmd, "get") == 0) {
    if (!a1) {
      push(p, "err=syntax");
      return;
    }
    char out[64];
    if (strcmp(a1, "rpm") == 0) {
      snprintf(out, sizeof(out), "rpm=%d", p->cfg.rpm);
    } else if (strcmp(a1, "ring_teeth") == 0) {
      snprintf(out, sizeof(out), "ring_teeth=%d", p->cfg.ring_teeth);
    } else if (strcmp(a1, "mute") == 0) {
      snprintf(out, sizeof(out), "mute=%d", p->cfg.mute);
    } else if (strcmp(a1, "route") == 0) {
      snprintf(out, sizeof(out), "route=%s", p->cfg.route);
    } else {
      push(p, "err=unknown");
      return;
    }
    push(p, out);
    return;
  }
  if (strcmp(cmd, "set") == 0) {
    if (!a1 || !a2 || xuss_config_set(&p->cfg, a1, a2) != 0) {
      push(p, "err=range");
      return;
    }
    if (strcmp(a1, "rpm") == 0) {
      p->active_rpm = p->cfg.rpm;
    }
    char out[64];
    snprintf(out, sizeof(out), "%s=%s", a1, a2);
    push(p, out);
    return;
  }
  if (strcmp(cmd, "defaults") == 0) {
    xuss_config_defaults(&p->cfg);
    p->active_rpm = 0;
    push(p, "ok");
    return;
  }
  if (strcmp(cmd, "rpm") == 0) {
    if (!a1) {
      char out[32];
      snprintf(out, sizeof(out), "rpm=%d", p->cfg.rpm);
      push(p, out);
      return;
    }
    if (xuss_config_set(&p->cfg, "rpm", a1) != 0) {
      push(p, "err=range");
      return;
    }
    p->active_rpm = p->cfg.rpm;
    char out[32];
    snprintf(out, sizeof(out), "rpm=%d", p->cfg.rpm);
    push(p, out);
    return;
  }
  if (strcmp(cmd, "stop") == 0) {
    p->cfg.rpm = 0;
    p->active_rpm = 0;
    push(p, "ok");
    return;
  }
  if (strcmp(cmd, "repl") == 0) {
    p->exit_repl = 1;
    push(p, "ok");
    return;
  }
  if (strcmp(cmd, "reboot") == 0) {
    p->do_reboot = 1;
    push(p, "ok");
    return;
  }
  push(p, "err=cmd");
}
