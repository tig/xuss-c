#ifndef GCU_PROTOCOL_H
#define GCU_PROTOCOL_H

#include "gcu/app.h"

/* ASCII line protocol on USB serial (§7). This is the complete Rev 0.3
 * command surface. Bounded intake, fail-closed on malformed input (§6.3). */

#define GCU_PROTO_MAX 64

typedef enum {
  GCU_PROTO_ERROR = 0, /* unknown / malformed / out of range */
  GCU_PROTO_IDENTITY,  /* caller emits identity (also written to out) */
  GCU_PROTO_REPL,      /* caller parks outputs, releases console */
  GCU_PROTO_REBOOT,    /* caller parks outputs, hard resets */
  GCU_PROTO_OK         /* commissioning handled; response in out */
} gcu_proto_result_t;

/* Parse one line already stripped of CR/LF. Writes a NUL-terminated response
 * into out (never overflows out_len). Mutates app->cfg for set/save/defaults. */
gcu_proto_result_t gcu_protocol_handle(gcu_app_t *app, const char *line,
                                       char *out, int out_len);

#endif
