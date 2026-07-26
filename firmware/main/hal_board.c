/* Device HAL backend — only TU allowlisted for device headers. */
#include "hal_board.h"

#include "boot_riff_pcm.h"
#include "font5x7.h"
#include "gcu/defaults.h"
#include "gcu/version.h"

#include "driver/dac_continuous.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "xussc";

#define LCD_HOST SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ (26 * 1000 * 1000)
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define HAIR_H 18
#define HINT_H 18
#define EYE_W 36
#define EYE_H 28

static gcu_hal_t board_hal;
static led_strip_handle_t s_strip;
static esp_lcd_panel_handle_t s_panel;
static dac_continuous_handle_t s_dac;
static SemaphoreHandle_t s_audio_mu;
static volatile int s_audio_playing;
static volatile int s_audio_pause_req;
static volatile int64_t s_audio_offset;
static volatile int s_want_boot_riff;
static volatile int s_want_full_play;
static int g_stdin_nonblock;
static int s_btn_last[3] = {1, 1, 1};
static int64_t s_btn_edge_ms[3];
static uint16_t *s_linebuf;
static int s_linebuf_w;

/* Measured M5GO IPS pack: R/B swapped into 565 word (see esp32-lcd-ips). */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3));
}

static void set_led_stub(gcu_hal_t *self, int on) {
  (void)self;
  (void)on;
  /* Product face uses side strip via board_apply_view, not GPIO2. */
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static int64_t now_ms(gcu_hal_t *self) {
  (void)self;
  return esp_timer_get_time() / 1000;
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
  if (!s_panel || w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > GCU_DISPLAY_W) {
    w = GCU_DISPLAY_W - x;
  }
  if (y + h > GCU_DISPLAY_H) {
    h = GCU_DISPLAY_H - y;
  }
  if (w <= 0 || h <= 0) {
    return;
  }
  if (!s_linebuf || s_linebuf_w < w) {
    if (s_linebuf) {
      free(s_linebuf);
    }
    s_linebuf = (uint16_t *)heap_caps_malloc((size_t)w * sizeof(uint16_t),
                                             MALLOC_CAP_DMA);
    s_linebuf_w = w;
  }
  if (!s_linebuf) {
    return;
  }
  for (int i = 0; i < w; i++) {
    s_linebuf[i] = color;
  }
  for (int row = 0; row < h; row++) {
    esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + 1,
                              s_linebuf);
  }
}

static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
  const uint8_t *g = font5x7_glyph(c);
  for (int col = 0; col < 5; col++) {
    uint8_t bits = g[col];
    for (int row = 0; row < 7; row++) {
      uint16_t color = (bits & (1u << row)) ? fg : bg;
      fill_rect(x + col * scale, y + row * scale, scale, scale, color);
    }
  }
}

static void draw_text(int x, int y, const char *s, uint16_t fg, uint16_t bg,
                      int scale) {
  if (!s) {
    return;
  }
  int cx = x;
  while (*s) {
    draw_char(cx, y, *s, fg, bg, scale);
    cx += 6 * scale;
    s++;
  }
}

static void draw_banner(const gcu_view_t *view, const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  fill_rect(0, 0, GCU_DISPLAY_W, HAIR_H, bg);
  draw_text(view->banner_offset_px, 4, GCU_BANNER_TEXT, ink, bg, 2);
}

static void draw_eye(int cx, int cy, int closed, uint16_t face, uint16_t bg) {
  fill_rect(cx - EYE_W / 2, cy - EYE_H / 2, EYE_W, EYE_H, bg);
  if (closed) {
    fill_rect(cx - EYE_W / 2, cy - 2, EYE_W, 4, face);
  } else {
    fill_rect(cx - EYE_W / 2 + 4, cy - EYE_H / 2 + 4, EYE_W - 8, EYE_H - 8,
              face);
    fill_rect(cx - 4, cy - 4, 8, 8, bg); /* pupil */
  }
}

static void draw_face_full(const gcu_view_t *view, const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t face = rgb565(th->face.r, th->face.g, th->face.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);

  fill_rect(0, 0, GCU_DISPLAY_W, GCU_DISPLAY_H, bg);
  draw_banner(view, th);

  int mid_y = HAIR_H + (GCU_DISPLAY_H - HAIR_H - HINT_H) / 2;
  draw_eye(110, mid_y - 10, 0, face, bg);
  draw_eye(210, mid_y - 10, view->wink_closed, face, bg);

  /* Smile */
  fill_rect(120, mid_y + 40, 80, 6, face);
  fill_rect(115, mid_y + 34, 10, 10, face);
  fill_rect(195, mid_y + 34, 10, 10, face);

  if (view->music == GCU_MUSIC_PLAYING) {
    draw_text(16, mid_y + 60, GCU_PLAYING_CUE, ink, bg, 1);
  }

  /* Button hints */
  fill_rect(0, GCU_DISPLAY_H - HINT_H, GCU_DISPLAY_W, HINT_H, bg);
  draw_text(12, GCU_DISPLAY_H - HINT_H + 4, "color", ink, bg, 1);
  draw_text(140, GCU_DISPLAY_H - HINT_H + 4,
            view->music == GCU_MUSIC_PLAYING ? "pause" : "play", ink, bg, 1);
  draw_text(250, GCU_DISPLAY_H - HINT_H + 4, "gear", ink, bg, 1);
}

static void draw_details(const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  char line[48];
  fill_rect(0, 0, GCU_DISPLAY_W, GCU_DISPLAY_H, bg);
  gcu_identity_line(line, (int)sizeof line);
  draw_text(8, 8, line, ink, bg, 1);
  draw_text(8, 28, "Details (IMU stub)", ink, bg, 1);
  draw_text(8, 48, "Tilt sensing next", ink, bg, 1);
  snprintf(line, sizeof line, "heap=%u", (unsigned)esp_get_free_heap_size());
  draw_text(8, 68, line, ink, bg, 1);
  draw_text(8, GCU_DISPLAY_H - 20, "A=face  B=play  C=noop", ink, bg, 1);
}

static void sides_apply(const gcu_view_t *view) {
  if (!s_strip) {
    return;
  }
  if (!view->sides_on) {
    led_strip_clear(s_strip);
    return;
  }
  for (int i = 0; i < GCU_SIDE_LED_COUNT; i++) {
    led_strip_set_pixel(s_strip, i, (uint32_t)view->sides_rgb_r,
                        (uint32_t)view->sides_rgb_g,
                        (uint32_t)view->sides_rgb_b);
  }
  led_strip_refresh(s_strip);
}

static void audio_task(void *arg) {
  (void)arg;
  uint8_t fade[256];
  for (;;) {
    if (s_want_boot_riff && s_dac) {
      s_want_boot_riff = 0;
      s_audio_playing = 1;
      xSemaphoreTake(s_audio_mu, portMAX_DELAY);
      /* Soft start: ease first samples toward mid if needed; stream riff. */
      esp_err_t err = dac_continuous_write(
          s_dac, (uint8_t *)BOOT_RIFF_PCM, BOOT_RIFF_PCM_LEN, NULL, -1);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "boot riff write: %s", esp_err_to_name(err));
      }
      /* Ease to mid then park low without destroying DAC session. */
      memset(fade, 128, sizeof fade);
      dac_continuous_write(s_dac, fade, sizeof fade, NULL, -1);
      memset(fade, 0, sizeof fade);
      dac_continuous_write(s_dac, fade, sizeof fade, NULL, -1);
      xSemaphoreGive(s_audio_mu);
      s_audio_playing = 0;
    }

    if (s_want_full_play && s_dac) {
      /* Full track not staged on FS yet — chirp + clear request. */
      s_want_full_play = 0;
      s_audio_playing = 1;
      xSemaphoreTake(s_audio_mu, portMAX_DELAY);
      /* Play boot riff as stand-in until first.pcm is on SPIFFS. */
      dac_continuous_write(s_dac, (uint8_t *)BOOT_RIFF_PCM, BOOT_RIFF_PCM_LEN,
                           NULL, -1);
      memset(fade, 128, sizeof fade);
      dac_continuous_write(s_dac, fade, sizeof fade, NULL, -1);
      memset(fade, 0, sizeof fade);
      dac_continuous_write(s_dac, fade, sizeof fade, NULL, -1);
      xSemaphoreGive(s_audio_mu);
      s_audio_playing = 0;
      s_audio_offset = 0;
    }

    if (s_audio_pause_req) {
      s_audio_pause_req = 0;
      /* Continuous write is blocking; pause is cooperative on next chunk. */
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void init_leds(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = GCU_PIN_SIDE_LED,
      .max_leds = GCU_SIDE_LED_COUNT,
      .led_model = LED_MODEL_SK6812,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .flags = {.invert_out = false},
  };
  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .flags = {.with_dma = false},
  };
  esp_err_t err =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "led_strip init failed: %s", esp_err_to_name(err));
    s_strip = NULL;
    return;
  }
  led_strip_clear(s_strip);
}

static void init_display(void) {
  gpio_config_t bk = {
      .pin_bit_mask = 1ULL << GCU_PIN_DISPLAY_BL,
      .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&bk);
  gpio_set_level(GCU_PIN_DISPLAY_BL, 1);

  spi_bus_config_t buscfg = {
      .sclk_io_num = GCU_PIN_DISPLAY_SCLK,
      .mosi_io_num = GCU_PIN_DISPLAY_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = GCU_DISPLAY_W * 40 * sizeof(uint16_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = GCU_PIN_DISPLAY_CS,
      .dc_gpio_num = GCU_PIN_DISPLAY_DC,
      .spi_mode = 0,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 10,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                           &io_config, &io));

  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = GCU_PIN_DISPLAY_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_config, &s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
  /* Bench: MADCTL BGR only, invert on (m5-core / esp32-lcd-ips). */
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
}

static void init_audio(void) {
  dac_continuous_config_t cont_cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0, /* GPIO25 = DAC1 */
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = GCU_AUDIO_SAMPLE_HZ,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_APLL,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  esp_err_t err = dac_continuous_new_channels(&cont_cfg, &s_dac);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "dac_continuous_new_channels: %s", esp_err_to_name(err));
    s_dac = NULL;
    return;
  }
  err = dac_continuous_enable(s_dac);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "dac_continuous_enable: %s", esp_err_to_name(err));
    s_dac = NULL;
  }
}

static void init_buttons(void) {
  gpio_config_t io = {
      .pin_bit_mask = (1ULL << GCU_PIN_BTN_A) | (1ULL << GCU_PIN_BTN_B) |
                      (1ULL << GCU_PIN_BTN_C),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE, /* external pull-ups on M5 */
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io);
}

static void stdin_set_nonblocking(void) {
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0) {
    g_stdin_nonblock = 0;
    return;
  }
  g_stdin_nonblock = (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0);
}

void board_init(void) {
  s_audio_mu = xSemaphoreCreateMutex();
  init_leds();
  init_display();
  init_audio();
  init_buttons();
  stdin_set_nonblocking();
  xTaskCreate(audio_task, "audio", 4096, NULL, 5, NULL);
}

gcu_hal_t *gcu_make_board_hal(void) {
  board_hal.set_led = set_led_stub;
  board_hal.delay_ms = delay_ms;
  board_hal.now_ms = now_ms;
  return &board_hal;
}

void board_apply_view(const gcu_view_t *view, const gcu_state_t *st) {
  if (!view) {
    return;
  }
  const gcu_theme_t *th = gcu_theme_at(view->theme_index);

  if (view->request_boot_riff) {
    s_want_boot_riff = 1;
  }
  if (view->request_play) {
    s_want_full_play = 1;
  }
  if (view->request_pause) {
    s_audio_pause_req = 1;
  }

  sides_apply(view);

  if (!s_panel) {
    return;
  }

  if (view->full_repaint) {
    if (view->screen == GCU_SCREEN_DETAILS) {
      draw_details(th);
    } else {
      draw_face_full(view, th);
    }
    return;
  }

  if (view->screen != GCU_SCREEN_FACE) {
    return;
  }

  if (view->banner_repaint) {
    draw_banner(view, th);
  }
  if (view->eye_repaint) {
    uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
    uint16_t face = rgb565(th->face.r, th->face.g, th->face.b);
    int mid_y = HAIR_H + (GCU_DISPLAY_H - HAIR_H - HINT_H) / 2;
    draw_eye(210, mid_y - 10, view->wink_closed, face, bg);
  }
  (void)st;
}

void board_poll_buttons(gcu_state_t *st) {
  if (!st || !st->hal || !st->hal->now_ms) {
    return;
  }
  int64_t now = st->hal->now_ms(st->hal);
  const int pins[3] = {GCU_PIN_BTN_A, GCU_PIN_BTN_B, GCU_PIN_BTN_C};
  const gcu_btn_t btns[3] = {GCU_BTN_A, GCU_BTN_B, GCU_BTN_C};
  for (int i = 0; i < 3; i++) {
    int level = gpio_get_level(pins[i]); /* active low */
    if (s_btn_last[i] == 1 && level == 0) {
      if (now - s_btn_edge_ms[i] >= (int64_t)GCU_DEFAULTS.btn_debounce_ms) {
        s_btn_edge_ms[i] = now;
        gcu_on_button(st, btns[i]);
      }
    }
    s_btn_last[i] = level;
  }
}

void board_park_outputs(void) {
  if (s_strip) {
    led_strip_clear(s_strip);
  }
  if (s_dac) {
    uint8_t z[64];
    memset(z, 0, sizeof z);
    xSemaphoreTake(s_audio_mu, portMAX_DELAY);
    dac_continuous_write(s_dac, z, sizeof z, NULL, 100);
    xSemaphoreGive(s_audio_mu);
  }
}

void board_hard_reset(void) {
  board_park_outputs();
  esp_restart();
}

void board_service_serial(gcu_state_t *st) {
  static char line[48];
  static int n;
  int c;

  (void)st;
  if (!g_stdin_nonblock) {
    return;
  }

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
        } else if (strcmp(p, "repl") == 0) {
          board_park_outputs();
          printf("ok repl parked\n");
          fflush(stdout);
          /* Stay in loop; host can flash. Identity still answers. */
        } else if (strcmp(p, "reboot") == 0) {
          printf("ok reboot\n");
          fflush(stdout);
          board_hard_reset();
        }
        n = 0;
      }
      continue;
    }
    if (n < (int)sizeof(line) - 1) {
      line[n++] = (char)c;
    } else {
      n = 0;
    }
  }
  clearerr(stdin);
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    errno = 0;
  }
}
