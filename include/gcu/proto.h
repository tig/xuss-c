#ifndef GCU_PROTO_H
#define GCU_PROTO_H

/* ASCII line protocol (spec.md §7) — pure logic, bounded intake,
 * fail-closed on malformed input. Complete Rev 0.3 surface:
 * identity, repl, reboot, get/set/save/defaults over
 * mute / volume / telemetry_hz. */

#define GCU_PROTO_LINE_MAX 96

typedef struct gcu_proto gcu_proto_t;

typedef void (*gcu_proto_emit_fn)(void *user, const char *line);
typedef void (*gcu_proto_act_fn)(void *user);

struct gcu_proto {
  char buf[GCU_PROTO_LINE_MAX];
  int len;
  int overflow; /* discarding until newline */

  /* commissioning params */
  int mute;
  int volume;
  int telemetry_hz;

  void *user;
  gcu_proto_emit_fn emit;      /* required: response lines */
  gcu_proto_act_fn on_repl;    /* park outputs, release console */
  gcu_proto_act_fn on_reboot;  /* park outputs, hard reset */
  gcu_proto_act_fn on_save;    /* optional: persist params */
};

void gcu_proto_init(gcu_proto_t *p, void *user, gcu_proto_emit_fn emit,
                    gcu_proto_act_fn on_repl, gcu_proto_act_fn on_reboot,
                    gcu_proto_act_fn on_save);

/* Feed raw link bytes; dispatches complete lines. Ctrl-C is data. */
void gcu_proto_feed(gcu_proto_t *p, const char *bytes, int n);

#endif
