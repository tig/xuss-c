#ifndef GCU_DEFAULTS_H
#define GCU_DEFAULTS_H

/* Shipped product defaults — metal and host tests share this one table.
 * Product code must read GCU_DEFAULTS, not parallel literals. */

/* Placeholder device tick (plate LED loop until L1 rendering lands). */
#define GCU_TICK_SLEEP_MS 250

/* Face timing (§4.2, §5.2 — wall-clock based). */
#define GCU_WINK_PERIOD_MS 10000 /* right-eye wink cadence */
#define GCU_WINK_CLOSE_MS 320    /* how long the eye stays closed */

/* Details refresh cadence (§4.5 — ~10 Hz visual). */
#define GCU_DETAILS_REFRESH_MS 100

/* Idle hair banner text (§4.2 — note the semicolon). */
#define GCU_BANNER_TEXT "Xuss-C; built on ESP-IDF"

/* Hair banner scroll speed (§4.2 — smooth right→left, wall-clock based). */
#define GCU_BANNER_SPEED_PX_S 60

/* UI animation frame pace (ms) for the living face loop. */
#define GCU_UI_FRAME_MS 30

/* Audio (§3 — unsigned 8-bit mono PCM; prefer 22050 Hz). */
#define GCU_SAMPLE_RATE_HZ 22050

/* Commissioning defaults (§7.2). */
#define GCU_VOLUME_DEFAULT 6 /* comfortable fixed desk level (0..10) */
#define GCU_MUTE_DEFAULT 0

typedef struct {
  int tick_sleep_ms;
  int wink_period_ms;
  int wink_close_ms;
  int details_refresh_ms;
  int sample_rate_hz;
  int volume;
  int mute;
  const char *banner_text;
} gcu_defaults_t;

/* Single shipped table (product path must use this, not parallel literals). */
extern const gcu_defaults_t GCU_DEFAULTS;

#endif
