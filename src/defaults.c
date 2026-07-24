#include "gcu/defaults.h"

const gcu_defaults_t GCU_DEFAULTS = {
    .tick_sleep_ms = GCU_TICK_SLEEP_MS,
    .wink_period_ms = GCU_WINK_PERIOD_MS,
    .wink_close_ms = GCU_WINK_CLOSE_MS,
    .details_refresh_ms = GCU_DETAILS_REFRESH_MS,
    .sample_rate_hz = GCU_SAMPLE_RATE_HZ,
    .volume = GCU_VOLUME_DEFAULT,
    .mute = GCU_MUTE_DEFAULT,
    .banner_text = GCU_BANNER_TEXT,
};
