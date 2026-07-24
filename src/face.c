#include "gcu/face.h"

int gcu_wink_is_closed(long now_ms, int period_ms, int close_ms) {
  if (period_ms <= 0 || close_ms <= 0 || now_ms < 0) {
    return 0;
  }
  long phase = now_ms % period_ms;
  return phase < close_ms ? 1 : 0;
}

int gcu_banner_offset(long now_ms, int speed_px_s, int span_px) {
  if (span_px <= 0 || speed_px_s <= 0 || now_ms < 0) {
    return 0;
  }
  long moved = (now_ms * (long)speed_px_s) / 1000;
  return (int)(moved % (long)span_px);
}
