#ifndef GCU_FACE_H
#define GCU_FACE_H

/* Wall-clock face animation helpers (§4.2, §5.2 — time-based, not tick-count).
 * Pure functions so host tests fully cover the timing. */

/* Right-eye wink: closed for the first `close_ms` of every `period_ms`.
 * Returns 1 when the eye should be drawn closed, else 0. */
int gcu_wink_is_closed(long now_ms, int period_ms, int close_ms);

/* Hair banner scroll: leftmost pixel offset for elapsed time given scroll
 * speed (px/sec) and total scroll span in px (text width + panel width).
 * Wraps within [0, span). */
int gcu_banner_offset(long now_ms, int speed_px_s, int span_px);

#endif
