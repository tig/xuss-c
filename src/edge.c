#include "xuss/edge.h"

#include <string.h>

double xuss_rpm_to_hz(int rpm, int ring_teeth) {
  return ((double)rpm * (double)ring_teeth) / 60.0;
}

static const xuss_profile_step_t k_crank[] = {
    {200, 800}, {433, 600}, {900, 400}, {750, 1200},
};

static const xuss_profile_step_t k_stall[] = {
    {750, 500}, {500, 400}, {250, 400}, {100, 500}, {0, 300},
};

/* redline: 200..4000 step 100, 80ms each — generated at query time via static table */
static xuss_profile_step_t k_redline[40];
static int k_redline_ready;

static void ensure_redline(void) {
  if (k_redline_ready) {
    return;
  }
  int i = 0;
  for (int rpm = 200; rpm <= 4000 && i < 40; rpm += 100) {
    k_redline[i].rpm = rpm;
    k_redline[i].ms = 80;
    i++;
  }
  k_redline_ready = i;
}

const xuss_profile_step_t *xuss_profile_steps(const char *name, size_t *out_len) {
  if (!name || !out_len) {
    return NULL;
  }
  if (strcmp(name, "crank_catch_idle") == 0) {
    *out_len = sizeof(k_crank) / sizeof(k_crank[0]);
    return k_crank;
  }
  if (strcmp(name, "stall") == 0) {
    *out_len = sizeof(k_stall) / sizeof(k_stall[0]);
    return k_stall;
  }
  if (strcmp(name, "redline_sweep") == 0) {
    ensure_redline();
    *out_len = (size_t)k_redline_ready;
    return k_redline;
  }
  *out_len = 0;
  return NULL;
}

int xuss_profile_total_ms(const xuss_profile_step_t *steps, size_t n) {
  int total = 0;
  for (size_t i = 0; i < n; i++) {
    total += steps[i].ms;
  }
  return total;
}

int xuss_profile_rpm_at(const xuss_profile_step_t *steps, size_t n, int t_ms) {
  if (!steps || n == 0) {
    return 0;
  }
  if (t_ms < 0) {
    return steps[0].rpm;
  }
  int elapsed = 0;
  int last = steps[0].rpm;
  for (size_t i = 0; i < n; i++) {
    last = steps[i].rpm;
    if (t_ms < elapsed + steps[i].ms) {
      return last;
    }
    elapsed += steps[i].ms;
  }
  return last;
}
