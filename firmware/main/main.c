#include "gcu/app.h"
#include "gcu/defaults.h"
#include "gcu/details.h"
#include "gcu/domain.h"
#include "gcu/face.h"
#include "gcu/hal.h"
#include "gcu/input.h"
#include "gcu/render.h"
#include "gcu/version.h"
#include "hal_board.h"
#include "hal_display.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Boot riff asset (spec §4.1): a ~2.5s slice of "First" as unsigned 8-bit
 * mono PCM @ 22050 Hz, embedded in the image (firmware/main/first_riff.u8,
 * see EMBED_FILES). Played once at boot, after the identity print.
 */
extern const uint8_t first_riff_start[] asm("_binary_first_riff_u8_start");
extern const uint8_t first_riff_end[] asm("_binary_first_riff_u8_end");

/*
 * Identity on the link (#78 / #79): boot-print alone is not enough for
 * silico inspect after the greeting scrolls past. The app must also answer
 * the host word "identity" (CR/LF framed) with fw_name=… fw_version=….
 *
 * stdin MUST be non-blocking before the forever loop. Blocking getchar()
 * would park app_main and kill the product face (tick/LED) until a host
 * line arrives.
 */
static int g_stdin_nonblock;

static void stdin_set_nonblocking(void) {
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0) {
    g_stdin_nonblock = 0;
    return;
  }
  if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0) {
    g_stdin_nonblock = 1;
  } else {
    g_stdin_nonblock = 0;
  }
}

static void drain_identity_command(void) {
  static char line[48];
  static int n;
  int c;

  if (!g_stdin_nonblock) {
    return; /* never block the product face */
  }

  /* Drain only ready bytes; empty stdin yields EOF/EAGAIN immediately. */
  while ((c = getchar()) != EOF) {
    if (c == '\r' || c == '\n') {
      if (n > 0) {
        line[n] = '\0';
        char *p = line;
        while (*p && isspace((unsigned char)*p)) {
          p++;
        }
        if (strcmp(p, "identity") == 0) {
          char id[64];
          gcu_identity_line(id, (int)sizeof id);
          printf("%s\n", id);
          fflush(stdout);
        }
        n = 0;
      }
      continue;
    }
    if (n < (int)sizeof(line) - 1) {
      line[n++] = (char)c;
    } else {
      n = 0; /* overflow: drop */
    }
  }
  /* Clear sticky errno from EAGAIN/EWOULDBLOCK after empty non-block read. */
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    errno = 0;
  }
}

void app_main(void) {
  char id[64];
  gcu_state_t st;
  /* HAL init must stay reachable from app_main (silico gate checks this).
   * Do not move the forever loop without gcu_make_board_hal + gcu_init (#79). */
  gcu_hal_t *hal = gcu_make_board_hal();
  gcu_gfx_t *gfx = gcu_make_display();

  gcu_identity_line(id, (int)sizeof id);
  printf("%s\n", id);
  fflush(stdout);

  /* Boot riff (§4.1): identity is printed first, then the riff plays once.
   * Blocks ~2.5s at boot; concurrent audio+face is a §5 follow-on. */
  if (hal && hal->play_pcm) {
    int riff_len = (int)(first_riff_end - first_riff_start);
    hal->play_pcm(hal, first_riff_start, riff_len, GCU_SAMPLE_RATE_HZ);
  }

  stdin_set_nonblocking();
  if (!g_stdin_nonblock) {
    printf("WARN: stdin not non-blocking; identity knock drain disabled "
           "(product face tick continues)\n");
    fflush(stdout);
  }

  gcu_init(&st, hal);

  /* Product UI (§4.2–§4.6): gcu_app_t is the source of truth; debounced button
   * edges drive it; the compositor paints regionally, full-clears only on mode
   * changes (theme switch, enter/leave Details). Concurrent audio tasks land in
   * the full-song stage. */
  gcu_app_t app;
  gcu_app_init(&app);
  gcu_debounce_t db;
  gcu_debounce_init(&db, 0);

  int span = gcu_render_banner_span(2);
  gcu_face_view_t view = {GCU_THEME_BLUE, 0, span ? 0 : 0, 0};

  gcu_screen_t last_screen = -1;   /* force first paint */
  gcu_theme_t last_theme = -1;
  int last_playing = -1;
  int last_wink = 0;
  long last_details_ms = 0;

  for (;;) {
    drain_identity_command();
    long now = (hal && hal->now_ms) ? hal->now_ms(hal) : 0;

    /* Input: sample + debounce -> app button edges (§4.3 one edge per press). */
    if (hal && hal->read_buttons) {
      int edges = gcu_debounce_update(&db, hal->read_buttons(hal), now, 25);
      if (edges & (1 << 0)) {
        gcu_app_button(&app, GCU_BTN_A);
      }
      if (edges & (1 << 1)) {
        gcu_app_button(&app, GCU_BTN_B);
      }
      if (edges & (1 << 2)) {
        gcu_app_button(&app, GCU_BTN_C);
      }
    }

    int playing = gcu_app_music_playing(&app);
    int mode_change = (app.screen != last_screen) ||
                      (app.screen == GCU_SCREEN_FACE && app.theme != last_theme);

    if (gfx && mode_change) {
      if (app.screen == GCU_SCREEN_FACE) {
        view.theme = app.theme;
        view.playing = playing;
        view.wink_closed = gcu_wink_is_closed(now, GCU_DEFAULTS.wink_period_ms,
                                              GCU_DEFAULTS.wink_close_ms);
        view.banner_offset =
            gcu_banner_offset(now, GCU_BANNER_SPEED_PX_S, span);
        gcu_render_face(gfx, &view);
        last_wink = view.wink_closed;
      } else {
        gcu_render_details_chrome(gfx, app.theme, id);
        last_details_ms = 0; /* force immediate value paint */
      }
      last_screen = app.screen;
      last_theme = app.theme;
      last_playing = playing;
    } else if (gfx && app.screen == GCU_SCREEN_FACE) {
      /* Routine face animation: regional paints only. */
      view.theme = app.theme;
      view.playing = playing;
      view.banner_offset = gcu_banner_offset(now, GCU_BANNER_SPEED_PX_S, span);
      gcu_render_hair(gfx, &view);
      int wc = gcu_wink_is_closed(now, GCU_DEFAULTS.wink_period_ms,
                                  GCU_DEFAULTS.wink_close_ms);
      if (wc != last_wink) {
        view.wink_closed = wc;
        gcu_render_eye(gfx, &view, 1);
        last_wink = wc;
      }
      if (playing != last_playing) {
        gcu_render_cue(gfx, &view);
        gcu_render_hints(gfx, &view);
        last_playing = playing;
      }
    } else if (gfx && app.screen == GCU_SCREEN_DETAILS) {
      /* Details value refresh (§4.5, ~10 Hz). Live sensors land next stage. */
      if (now - last_details_ms >= GCU_DEFAULTS.details_refresh_ms) {
        gcu_details_t d;
        memset(&d, 0, sizeof d);
        if (hal && hal->read_buttons) {
          int m = hal->read_buttons(hal);
          d.button[0] = (m >> 0) & 1;
          d.button[1] = (m >> 1) & 1;
          d.button[2] = (m >> 2) & 1;
        }
        gcu_render_details_values(gfx, app.theme, &d);
        last_details_ms = now;
      }
    }

    if (hal && hal->delay_ms) {
      hal->delay_ms(hal, GCU_UI_FRAME_MS);
    }
  }
}
