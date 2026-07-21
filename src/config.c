#include "xuss/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xuss_config_factory(xuss_config_t *cfg) {
  if (!cfg) {
    return;
  }
  memset(cfg, 0, sizeof(*cfg));
  cfg->ring_teeth = 130;
  cfg->rpm = 0;
  cfg->duty_pct = 50;
  strncpy(cfg->route, "voice", sizeof(cfg->route) - 1);
  cfg->volume = 6;
  cfg->greet = 1;
  cfg->knob = 0;
  cfg->mute = 0;
  cfg->telemetry_hz = 0;
}

static int parse_int(const char *s, int *out) {
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (!s || end == s) {
    return -1;
  }
  *out = (int)v;
  return 0;
}

int xuss_config_set(xuss_config_t *cfg, const char *key, const char *value) {
  if (!cfg || !key || !value) {
    return -1;
  }
  int v = 0;
  if (strcmp(key, "ring_teeth") == 0) {
    if (parse_int(value, &v) || v < 10 || v > 400) {
      return -1;
    }
    cfg->ring_teeth = v;
    return 0;
  }
  if (strcmp(key, "rpm") == 0) {
    if (parse_int(value, &v) || v < 0 || v > 8000) {
      return -1;
    }
    cfg->rpm = v;
    return 0;
  }
  if (strcmp(key, "duty_pct") == 0) {
    if (parse_int(value, &v) || v < 5 || v > 95) {
      return -1;
    }
    cfg->duty_pct = v;
    return 0;
  }
  if (strcmp(key, "route") == 0) {
    if (strcmp(value, "voice") && strcmp(value, "tach") && strcmp(value, "both")) {
      return -1;
    }
    strncpy(cfg->route, value, sizeof(cfg->route) - 1);
    return 0;
  }
  if (strcmp(key, "volume") == 0) {
    if (parse_int(value, &v) || v < 0 || v > 10) {
      return -1;
    }
    cfg->volume = v;
    return 0;
  }
  if (strcmp(key, "greet") == 0 || strcmp(key, "knob") == 0 || strcmp(key, "mute") == 0) {
    if (parse_int(value, &v) || (v != 0 && v != 1)) {
      return -1;
    }
    if (key[0] == 'g') {
      cfg->greet = v;
    } else if (key[0] == 'k') {
      cfg->knob = v;
    } else {
      cfg->mute = v;
    }
    return 0;
  }
  if (strcmp(key, "telemetry_hz") == 0) {
    if (parse_int(value, &v) || v < 0 || v > 100) {
      return -1;
    }
    cfg->telemetry_hz = v;
    return 0;
  }
  return -1;
}

void xuss_config_defaults(xuss_config_t *cfg) {
  int mute = cfg ? cfg->mute : 0;
  xuss_config_factory(cfg);
  if (cfg) {
    cfg->mute = mute;
  }
}
