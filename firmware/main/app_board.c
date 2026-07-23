/* App composition root (device glue) — allowlisted for device headers.
 * Owns buttons, the link (UART0), and the UI loop; audio runs in its own
 * task (audio_board). Domain logic stays in host-importable src/ modules. */
#include "app_board.h"

#include "gcu/defaults.h"
#include "gcu/input.h"
#include "gcu/proto.h"
#include "gcu/ui.h"

#include "audio_board.h"
#include "hal_board.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

#define AUDIO_PARK 100 /* audio_board request: not a gcu_audio_req_t */

static int repl_parked; /* escape hatch: outputs parked, console released */

static unsigned now_ms(void) {
  return (unsigned)(esp_timer_get_time() / 1000);
}

static void park_outputs(void) {
  gcu_board_audio_request(AUDIO_PARK);
  gcu_board_led_theme(GCU_THEME_BLACK); /* sides off */
}

static void emit_line(void *user, const char *line) {
  (void)user;
  printf("%s\n", line);
}

static void on_repl(void *user) {
  (void)user;
  park_outputs();
  repl_parked = 1; /* product releases outputs; link keeps answering */
}

static void on_reboot(void *user) {
  (void)user;
  park_outputs();
  vTaskDelay(pdMS_TO_TICKS(50)); /* let "ok reboot" drain */
  esp_restart();
}

static void buttons_init(void) {
  int pins[GCU_INPUT_N] = {GCU_DEFAULTS.btn_a_pin, GCU_DEFAULTS.btn_b_pin,
                           GCU_DEFAULTS.btn_c_pin};
  for (int i = 0; i < GCU_INPUT_N; i++) {
    gpio_reset_pin(pins[i]);
    gpio_set_direction(pins[i], GPIO_MODE_INPUT);
    /* GPIO 34-39: no internal pulls; the board has external pullups. */
  }
}

static void buttons_read(unsigned char pressed[GCU_INPUT_N]) {
  pressed[0] = gpio_get_level(GCU_DEFAULTS.btn_a_pin) == 0;
  pressed[1] = gpio_get_level(GCU_DEFAULTS.btn_b_pin) == 0;
  pressed[2] = gpio_get_level(GCU_DEFAULTS.btn_c_pin) == 0;
}

void gcu_board_app_run(unsigned song_len) {
  gcu_ui_t ui;
  gcu_input_t input;
  gcu_proto_t proto;
  char rx[256]; /* per-turn intake must beat the 115200 line rate */

  gcu_make_board_hal(); /* initializes the side strip (review P0: was dropped
                           in the app rewrite — theme LEDs were dead) */
  buttons_init();
  esp_err_t link_err = uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
  printf("link=%s\n", link_err == ESP_OK ? "ok" : esp_err_to_name(link_err));

  gcu_board_audio_task_start(song_len);
  /* Boot greeting via the audio task: the riff plays while this loop is
   * already servicing the link (spec §4.1/§5.2.4 — identity printed by
   * app_main before this; the link never waits on audio). */
  gcu_board_audio_request(GCU_BOARD_AUDIO_RIFF);
  gcu_proto_init(&proto, NULL, emit_line, on_repl, on_reboot, NULL);
  gcu_ui_init(&ui, now_ms());
  gcu_input_init(&input, now_ms());
  gcu_board_led_theme(ui.theme);
  gcu_ui_take_dirty(&ui);

  for (;;) {
    unsigned now = now_ms();

    /* Link first: bounded intake per turn; works mid-song (§5.2, §6.3). */
    int n = uart_read_bytes(UART_NUM_0, rx, sizeof rx, 0);
    if (n > 0) {
      gcu_proto_feed(&proto, rx, n);
    }

    if (repl_parked) {
      /* Flashable, host-serviceable state: no product outputs; the link
       * still answers (identity/reboot) so a host can redeploy. */
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    /* Commissioning params into the running product. */
    ui.mute = proto.mute;
    gcu_board_audio_set_volume(proto.volume);

    /* Buttons -> UI -> audio requests. */
    unsigned char pressed[GCU_INPUT_N];
    buttons_read(pressed);
    unsigned edges = gcu_input_update(&input, pressed, now);
    for (int b = 0; b < GCU_INPUT_N; b++) {
      if (!(edges & (1u << b))) {
        continue;
      }
      gcu_audio_req_t req = gcu_ui_on_button(&ui, (gcu_button_t)b, now);
      if (req == GCU_AUDIO_START && song_len == 0) {
        printf("audio=missing\n"); /* refuse clearly; UI stays usable */
        gcu_ui_song_error(&ui);
      } else if (req != GCU_AUDIO_NONE) {
        gcu_board_audio_request((int)req);
      }
      if (ui.start_blocked) {
        printf("err muted\n");
        ui.start_blocked = 0;
      }
    }

    /* Audio task events. */
    int ev = gcu_board_audio_poll_event();
    if (ev == 1) {
      gcu_ui_song_ended(&ui);
    } else if (ev == 2) {
      printf("audio=error\n");
      gcu_ui_song_error(&ui);
    }

    /* Time-based UI schedule; consume paints the board can show today
     * (side LEDs; the IPS face arrives with the display slice, #5). */
    gcu_ui_tick(&ui, now);
    unsigned dirty = gcu_ui_take_dirty(&ui);
    if (dirty & (GCU_DIRTY_THEME | GCU_DIRTY_SCREEN)) {
      gcu_board_led_theme(ui.theme);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
