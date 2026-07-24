#include "gcu/details.h"

#include <stdio.h>

int gcu_fmt_milli1(char *out, int out_len, long milli) {
  long a, whole, frac;
  const char *sign;
  int n;

  if (!out || out_len < 5) {
    return 0;
  }
  sign = milli < 0 ? "-" : "";
  a = milli < 0 ? -milli : milli;
  whole = a / 1000;
  frac = (a % 1000) / 100; /* single fractional digit, truncated */
  n = snprintf(out, (size_t)out_len, "%s%ld.%ld", sign, whole, frac);
  if (n < 0 || n >= out_len) {
    return 0;
  }
  return n;
}
