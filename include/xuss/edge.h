#ifndef XUSS_EDGE_H
#define XUSS_EDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** f_hz = rpm * ring_teeth / 60.0 */
double xuss_rpm_to_hz(int rpm, int ring_teeth);

typedef struct {
  int rpm;
  int ms;
} xuss_profile_step_t;

const xuss_profile_step_t *xuss_profile_steps(const char *name, size_t *out_len);
int xuss_profile_rpm_at(const xuss_profile_step_t *steps, size_t n, int t_ms);
int xuss_profile_total_ms(const xuss_profile_step_t *steps, size_t n);

#ifdef __cplusplus
}
#endif

#endif
