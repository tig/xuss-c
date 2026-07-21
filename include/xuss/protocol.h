#ifndef XUSS_PROTOCOL_H
#define XUSS_PROTOCOL_H

#include "xuss/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XUSS_FW_NAME "XUSSC"
#define XUSS_FW_VERSION "0.0.1"

typedef struct {
  xuss_config_t cfg;
  int active_rpm;
  int exit_repl;
  int do_reboot;
  char last_resp[8][96];
  int n_resp;
} xuss_proto_t;

void xuss_proto_init(xuss_proto_t *p);
/** Handle one command line (no newline). Fills last_resp. */
void xuss_proto_handle(xuss_proto_t *p, const char *line);
void xuss_proto_identity(char *out, int out_len);

#ifdef __cplusplus
}
#endif

#endif
