#include "gcu/ui.h"

#include "gcu/defaults.h"

/* Wall-clock comparison tolerant of wrap (unsigned ms). */
static int due(unsigned now, unsigned at) { return (int)(now - at) >= 0; }

void gcu_ui_init(gcu_ui_t *ui, unsigned now_ms) {
  ui->screen = GCU_SCREEN_FACE;
  ui->theme = GCU_THEME_BLUE;
  ui->song = GCU_SONG_IDLE;
  ui->mute = 0;
  ui->start_blocked = 0;
  ui->banner_step = 0;
  ui->eye_closed = 0;
  ui->wink_due_ms = now_ms + (unsigned)GCU_DEFAULTS.wink_period_ms;
  ui->wink_open_ms = 0;
  ui->banner_due_ms = now_ms + (unsigned)GCU_DEFAULTS.banner_step_ms;
  ui->details_due_ms = now_ms;
  ui->dirty = GCU_DIRTY_SCREEN;
}

static gcu_audio_req_t press_b(gcu_ui_t *ui) {
  switch (ui->song) {
    case GCU_SONG_IDLE:
      if (ui->mute) {
        ui->start_blocked = 1;
        return GCU_AUDIO_NONE;
      }
      ui->song = GCU_SONG_PLAYING;
      ui->dirty |= GCU_DIRTY_CUE;
      return GCU_AUDIO_START;
    case GCU_SONG_PLAYING:
      ui->song = GCU_SONG_PAUSED;
      ui->dirty |= GCU_DIRTY_CUE;
      return GCU_AUDIO_PAUSE;
    case GCU_SONG_PAUSED:
    default:
      ui->song = GCU_SONG_PLAYING;
      ui->dirty |= GCU_DIRTY_CUE;
      return GCU_AUDIO_RESUME;
  }
}

gcu_audio_req_t gcu_ui_on_button(gcu_ui_t *ui, gcu_button_t btn,
                                 unsigned now_ms) {
  (void)now_ms;
  switch (btn) {
    case GCU_BTN_A:
      if (ui->screen == GCU_SCREEN_DETAILS) {
        /* Exit to face; no theme change; music state unchanged (§4.5). */
        ui->screen = GCU_SCREEN_FACE;
        ui->dirty |= GCU_DIRTY_SCREEN;
        return GCU_AUDIO_NONE;
      }
      if (ui->song == GCU_SONG_PLAYING) {
        /* Pause, stay on face, do not advance theme (§4.3). */
        ui->song = GCU_SONG_PAUSED;
        ui->dirty |= GCU_DIRTY_CUE;
        return GCU_AUDIO_PAUSE;
      }
      ui->theme = (ui->theme + 1) % GCU_THEME_COUNT;
      ui->dirty |= GCU_DIRTY_THEME;
      return GCU_AUDIO_NONE;

    case GCU_BTN_B:
      return press_b(ui); /* same rules on face and Details (§4.4/§4.5) */

    case GCU_BTN_C:
    default:
      if (ui->screen == GCU_SCREEN_FACE) {
        /* Open Details immediately; never force a pause (§4.5). */
        ui->screen = GCU_SCREEN_DETAILS;
        ui->dirty |= GCU_DIRTY_SCREEN;
      }
      /* On Details: no-op. */
      return GCU_AUDIO_NONE;
  }
}

void gcu_ui_tick(gcu_ui_t *ui, unsigned now_ms) {
  /* Wink: time-based period; repaint only the eye (§4.2). */
  if (!ui->eye_closed && due(now_ms, ui->wink_due_ms)) {
    ui->eye_closed = 1;
    ui->wink_open_ms = now_ms + (unsigned)GCU_DEFAULTS.wink_hold_ms;
    ui->wink_due_ms = now_ms + (unsigned)GCU_DEFAULTS.wink_period_ms;
    if (ui->screen == GCU_SCREEN_FACE) {
      ui->dirty |= GCU_DIRTY_EYE;
    }
  } else if (ui->eye_closed && due(now_ms, ui->wink_open_ms)) {
    ui->eye_closed = 0;
    if (ui->screen == GCU_SCREEN_FACE) {
      ui->dirty |= GCU_DIRTY_EYE;
    }
  }

  /* Banner scroll: continues while music plays (§4.2). */
  if (due(now_ms, ui->banner_due_ms)) {
    ui->banner_step += 1;
    ui->banner_due_ms = now_ms + (unsigned)GCU_DEFAULTS.banner_step_ms;
    if (ui->screen == GCU_SCREEN_FACE) {
      ui->dirty |= GCU_DIRTY_BANNER;
    }
  }

  /* Details values: ~100 ms visual cadence, only when visible (§4.5). */
  if (ui->screen == GCU_SCREEN_DETAILS && due(now_ms, ui->details_due_ms)) {
    ui->details_due_ms = now_ms + (unsigned)GCU_DEFAULTS.details_refresh_ms;
    ui->dirty |= GCU_DIRTY_DETAILS;
  }
}

void gcu_ui_song_ended(gcu_ui_t *ui) {
  ui->song = GCU_SONG_IDLE; /* offset back to 0 is backend-owned; no loop */
  ui->dirty |= GCU_DIRTY_CUE;
}

void gcu_ui_song_error(gcu_ui_t *ui) {
  ui->song = GCU_SONG_IDLE;
  ui->dirty |= GCU_DIRTY_CUE;
}

unsigned gcu_ui_take_dirty(gcu_ui_t *ui) {
  unsigned d = ui->dirty;
  ui->dirty = 0;
  return d;
}
