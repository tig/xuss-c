#ifndef XUSS_CONFIG_H
#define XUSS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int ring_teeth;
  int rpm;
  int duty_pct;
  char route[8]; /* voice|tach|both */
  int volume;
  int greet;
  int knob;
  int mute;
  int telemetry_hz;
} xuss_config_t;

void xuss_config_factory(xuss_config_t *cfg);
/** Returns 0 on success, non-zero on range/unknown. */
int xuss_config_set(xuss_config_t *cfg, const char *key, const char *value);
/** Restore factory except mute. */
void xuss_config_defaults(xuss_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
