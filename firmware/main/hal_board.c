/* Device HAL backend — only TU allowlisted for device headers. */
#include "hal_board.h"

#include "boot_riff_pcm.h"
#include "font5x7.h"
#include "gcu/defaults.h"
#include "gcu/version.h"

#include "driver/dac_continuous.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_spiffs.h"
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

#define I2C_PORT I2C_NUM_0
#define MPU_ADDR 0x68
#define MPU_WHO_AM_I_REG 0x75
#define MPU_WHO_AM_I_VAL 0x19
#define MPU_PWR_MGMT_1 0x6B
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_TEMP_OUT_H 0x41
#define MPU_GYRO_XOUT_H 0x43

#define AUDIO_CHUNK 2048
#define SONG_PATH "/spiffs/first.pcm"

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
static volatile int s_music_ended;
static volatile int s_music_missing;
static volatile int64_t s_song_size;
static int g_stdin_nonblock;
static int s_btn_last[3] = {1, 1, 1};
static int64_t s_btn_edge_ms[3];
static uint16_t *s_linebuf;
static int s_linebuf_w;
static uint16_t *s_strip_fb; /* hair or hint strip: W * strip_h DMA pixels */
static int s_strip_fb_h;
/* Full-frame shadow (panel BE RGB565) for host screenshots without a camera. */
static uint16_t *s_shadow;
static int s_shadow_ok;
static int s_imu_ok;
static int64_t s_last_details_ms;
static int s_spiffs_ok;
static int s_last_music_for_hints = -1;

#define SHADOW_PIXELS ((size_t)GCU_DISPLAY_W * (size_t)GCU_DISPLAY_H)
#define SHADOW_BYTES (SHADOW_PIXELS * sizeof(uint16_t))

/*
 * Logical RGB → panel pixel.
 *
 * Side SK6812s take the same logical (r,g,b) via led_strip_set_pixel (driver
 * packs GRB on the wire). Panel path must match those primaries.
 *
 * MADCTL BGR is set via LCD_RGB_ELEMENT_ORDER_BGR — do NOT also R↔B in the
 * 565 pack (double-swap makes screen disagree with the side strips).
 * SPI wants big-endian 565 words; ESP32 is LE so swap bytes for DMA.
 */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t c =
      (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
  return (uint16_t)((c >> 8) | (c << 8));
}

static void set_led_stub(gcu_hal_t *self, int on) {
  (void)self;
  (void)on;
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static int64_t now_ms(gcu_hal_t *self) {
  (void)self;
  return esp_timer_get_time() / 1000;
}

static int ensure_linebuf(int w) {
  if (w <= 0) {
    return 0;
  }
  if (!s_linebuf || s_linebuf_w < w) {
    if (s_linebuf) {
      free(s_linebuf);
    }
    s_linebuf = (uint16_t *)heap_caps_malloc((size_t)w * sizeof(uint16_t),
                                             MALLOC_CAP_DMA);
    s_linebuf_w = s_linebuf ? w : 0;
  }
  return s_linebuf != NULL;
}

static int ensure_strip_fb(int h) {
  if (h <= 0) {
    return 0;
  }
  if (!s_strip_fb || s_strip_fb_h < h) {
    if (s_strip_fb) {
      free(s_strip_fb);
    }
    size_t n = (size_t)GCU_DISPLAY_W * (size_t)h;
    s_strip_fb =
        (uint16_t *)heap_caps_malloc(n * sizeof(uint16_t), MALLOC_CAP_DMA);
    s_strip_fb_h = s_strip_fb ? h : 0;
  }
  return s_strip_fb != NULL;
}

static int ensure_shadow(void) {
  if (s_shadow_ok && s_shadow) {
    return 1;
  }
  /* Prefer internal 8-bit capable heap; fall back to any. */
  s_shadow = (uint16_t *)heap_caps_malloc(SHADOW_BYTES, MALLOC_CAP_8BIT);
  if (!s_shadow) {
    s_shadow = (uint16_t *)malloc(SHADOW_BYTES);
  }
  if (!s_shadow) {
    ESP_LOGW(TAG, "shadow FB alloc failed (%u bytes)", (unsigned)SHADOW_BYTES);
    s_shadow_ok = 0;
    return 0;
  }
  memset(s_shadow, 0, SHADOW_BYTES);
  s_shadow_ok = 1;
  ESP_LOGI(TAG, "shadow FB %ux%u ready (%u bytes)", GCU_DISPLAY_W, GCU_DISPLAY_H,
           (unsigned)SHADOW_BYTES);
  return 1;
}

static void shadow_store_row(int x, int y, int w, const uint16_t *px) {
  if (!s_shadow_ok || !s_shadow || !px || y < 0 || y >= GCU_DISPLAY_H || w <= 0) {
    return;
  }
  if (x < 0) {
    px += -x;
    w += x;
    x = 0;
  }
  if (x + w > GCU_DISPLAY_W) {
    w = GCU_DISPLAY_W - x;
  }
  if (w <= 0) {
    return;
  }
  memcpy(&s_shadow[y * GCU_DISPLAY_W + x], px, (size_t)w * sizeof(uint16_t));
}

static void blit_bitmap(int x, int y, int w, int h, const uint16_t *px) {
  if (!px || w <= 0 || h <= 0) {
    return;
  }
  /* Always keep shadow in sync when allocated (even if panel missing). */
  if (s_shadow_ok && s_shadow) {
    for (int row = 0; row < h; row++) {
      int py = y + row;
      if (py < 0 || py >= GCU_DISPLAY_H) {
        continue;
      }
      shadow_store_row(x, py, w, px + (size_t)row * (size_t)w);
    }
  }
  if (s_panel) {
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, px);
  }
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) {
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
  if (w <= 0 || h <= 0 || !ensure_linebuf(w)) {
    return;
  }
  for (int i = 0; i < w; i++) {
    s_linebuf[i] = color;
  }
  /* Batch solid rows through one-line DMA buffer (still solid, not per-pixel). */
  for (int row = 0; row < h; row++) {
    blit_bitmap(x, y + row, w, 1, s_linebuf);
  }
}

/* Host screenshot: text header + raw BE rgb565 + trailer on the console link. */
static void board_send_shot(void) {
  if (!ensure_shadow()) {
    printf("SHOT_ERR no_shadow\n");
    fflush(stdout);
    return;
  }
  const size_t nbytes = SHADOW_BYTES;
  uint32_t crc =
      esp_rom_crc32_le(0, (const uint8_t *)s_shadow, (uint32_t)nbytes);
  printf("SHOT w=%d h=%d fmt=rgb565be nbytes=%u crc=0x%08lx\n", GCU_DISPLAY_W,
         GCU_DISPLAY_H, (unsigned)nbytes, (unsigned long)crc);
  fflush(stdout);
  /*
   * Binary on the same VFS stdout as printf (USB CDC / UART console).
   * uart_write_bytes(UART_NUM_0) can miss the actual console backend.
   */
  const uint8_t *p = (const uint8_t *)s_shadow;
  size_t left = nbytes;
  while (left > 0) {
    size_t chunk = left > 2048 ? 2048 : left;
    ssize_t n = write(STDOUT_FILENO, p, chunk);
    if (n <= 0) {
      printf("\nSHOT_ERR write errno=%d\n", errno);
      fflush(stdout);
      return;
    }
    p += (size_t)n;
    left -= (size_t)n;
  }
  fflush(stdout);
  printf("\nSHOT_END crc=0x%08lx\n", (unsigned long)crc);
  fflush(stdout);
}

/* Compose 5x7 glyph into a strip framebuffer (stride = GCU_DISPLAY_W). */
static void fb_draw_char(uint16_t *fb, int fb_h, int x, int y, char c,
                         uint16_t fg, uint16_t bg, int scale) {
  const uint8_t *g = font5x7_glyph(c);
  for (int col = 0; col < 5; col++) {
    uint8_t bits = g[col];
    for (int row = 0; row < 7; row++) {
      uint16_t color = (bits & (1u << row)) ? fg : bg;
      for (int sy = 0; sy < scale; sy++) {
        int py = y + row * scale + sy;
        if (py < 0 || py >= fb_h) {
          continue;
        }
        for (int sx = 0; sx < scale; sx++) {
          int px = x + col * scale + sx;
          if (px < 0 || px >= GCU_DISPLAY_W) {
            continue;
          }
          fb[py * GCU_DISPLAY_W + px] = color;
        }
      }
    }
  }
}

static void fb_draw_text(uint16_t *fb, int fb_h, int x, int y, const char *s,
                         uint16_t fg, uint16_t bg, int scale) {
  if (!s || !fb) {
    return;
  }
  int cx = x;
  while (*s) {
    fb_draw_char(fb, fb_h, cx, y, *s, fg, bg, scale);
    cx += 6 * scale;
    s++;
  }
}

/* Direct panel text: compose one glyph row-run into linebuf (faster than pixels). */
static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
  const uint8_t *g = font5x7_glyph(c);
  int gw = 5 * scale;
  if (!ensure_linebuf(gw > 0 ? gw : 1)) {
    return;
  }
  for (int row = 0; row < 7; row++) {
    for (int sy = 0; sy < scale; sy++) {
      for (int col = 0; col < 5; col++) {
        uint16_t color = (g[col] & (1u << row)) ? fg : bg;
        for (int sx = 0; sx < scale; sx++) {
          s_linebuf[col * scale + sx] = color;
        }
      }
      int py = y + row * scale + sy;
      if (py >= 0 && py < GCU_DISPLAY_H && x < GCU_DISPLAY_W) {
        int draw_w = gw;
        int draw_x = x;
        if (draw_x < 0) {
          draw_w += draw_x;
          draw_x = 0;
        }
        if (draw_x + draw_w > GCU_DISPLAY_W) {
          draw_w = GCU_DISPLAY_W - draw_x;
        }
        if (draw_w > 0) {
          blit_bitmap(draw_x, py, draw_w, 1,
                      s_linebuf + (draw_x > x ? (draw_x - x) : 0));
        }
      }
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
    if (cx + 6 * scale > 0 && cx < GCU_DISPLAY_W) {
      draw_char(cx, y, *s, fg, bg, scale);
    }
    cx += 6 * scale;
    s++;
  }
}

/*
 * Spec § face hints: left "color" text; middle play/pause *symbol*;
 * right gear *symbol* — not the words "play"/"pause"/"gear".
 */
static void fb_icon_play(uint16_t *fb, int fb_h, int cx, int cy, uint16_t fg) {
  /* Right-pointing triangle ~12x12. */
  for (int y = -6; y <= 6; y++) {
    int half = (6 - (y < 0 ? -y : y));
    if (half < 1) {
      half = 1;
    }
    for (int x = -2; x <= half; x++) {
      int px = cx + x;
      int py = cy + y;
      if (px >= 0 && px < GCU_DISPLAY_W && py >= 0 && py < fb_h) {
        fb[py * GCU_DISPLAY_W + px] = fg;
      }
    }
  }
}

static void fb_icon_pause(uint16_t *fb, int fb_h, int cx, int cy, uint16_t fg) {
  for (int y = -6; y <= 6; y++) {
    for (int x = -6; x <= -3; x++) {
      int px = cx + x;
      int py = cy + y;
      if (px >= 0 && px < GCU_DISPLAY_W && py >= 0 && py < fb_h) {
        fb[py * GCU_DISPLAY_W + px] = fg;
      }
    }
    for (int x = 3; x <= 6; x++) {
      int px = cx + x;
      int py = cy + y;
      if (px >= 0 && px < GCU_DISPLAY_W && py >= 0 && py < fb_h) {
        fb[py * GCU_DISPLAY_W + px] = fg;
      }
    }
  }
}

static void fb_icon_gear(uint16_t *fb, int fb_h, int cx, int cy, uint16_t fg) {
  /* Simple gear: outer ring + four notches + hub. */
  for (int y = -7; y <= 7; y++) {
    for (int x = -7; x <= 7; x++) {
      int r2 = x * x + y * y;
      int on = 0;
      if (r2 >= 16 && r2 <= 49) {
        on = 1;
      }
      /* teeth on axes */
      if ((x >= -2 && x <= 2 && y >= -8 && y <= 8) ||
          (y >= -2 && y <= 2 && x >= -8 && x <= 8)) {
        if (r2 <= 64 && r2 >= 25) {
          on = 1;
        }
      }
      if (r2 <= 6) {
        on = 1; /* hub */
      }
      if (on) {
        int px = cx + x;
        int py = cy + y;
        if (px >= 0 && px < GCU_DISPLAY_W && py >= 0 && py < fb_h) {
          fb[py * GCU_DISPLAY_W + px] = fg;
        }
      }
    }
  }
}

/* Spec: hair-bar only; compose in RAM → one SPI blit (smooth scroll). */
static void draw_banner(const gcu_view_t *view, const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  if (!ensure_strip_fb(HAIR_H)) {
    return;
  }
  size_t n = (size_t)GCU_DISPLAY_W * (size_t)HAIR_H;
  for (size_t i = 0; i < n; i++) {
    s_strip_fb[i] = bg;
  }
  /* scale 2; vertical center-ish at y=2 */
  fb_draw_text(s_strip_fb, HAIR_H, view->banner_offset_px, 2, GCU_BANNER_TEXT,
               ink, bg, 2);
  blit_bitmap(0, 0, GCU_DISPLAY_W, HAIR_H, s_strip_fb);
}

static void draw_button_hints(const gcu_view_t *view, const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  if (!ensure_strip_fb(HINT_H)) {
    return;
  }
  size_t n = (size_t)GCU_DISPLAY_W * (size_t)HINT_H;
  for (size_t i = 0; i < n; i++) {
    s_strip_fb[i] = bg;
  }
  /* Left: word "color" above button A. Middle/right: glyphs per spec. */
  fb_draw_text(s_strip_fb, HINT_H, 18, 5, "color", ink, bg, 1);
  int mid_x = GCU_DISPLAY_W / 2;
  int right_x = (GCU_DISPLAY_W * 5) / 6;
  int cy = HINT_H / 2;
  if (view->music == GCU_MUSIC_PLAYING) {
    fb_icon_pause(s_strip_fb, HINT_H, mid_x, cy, ink);
  } else {
    fb_icon_play(s_strip_fb, HINT_H, mid_x, cy, ink);
  }
  fb_icon_gear(s_strip_fb, HINT_H, right_x, cy, ink);
  blit_bitmap(0, GCU_DISPLAY_H - HINT_H, GCU_DISPLAY_W, HINT_H, s_strip_fb);
  s_last_music_for_hints = (int)view->music;
}

static void draw_eye(int cx, int cy, int closed, uint16_t face, uint16_t bg) {
  fill_rect(cx - EYE_W / 2, cy - EYE_H / 2, EYE_W, EYE_H, bg);
  if (closed) {
    fill_rect(cx - EYE_W / 2, cy - 2, EYE_W, 4, face);
  } else {
    fill_rect(cx - EYE_W / 2 + 4, cy - EYE_H / 2 + 4, EYE_W - 8, EYE_H - 8,
              face);
    fill_rect(cx - 4, cy - 4, 8, 8, bg);
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

  fill_rect(120, mid_y + 40, 80, 6, face);
  fill_rect(115, mid_y + 34, 10, 10, face);
  fill_rect(195, mid_y + 34, 10, 10, face);

  if (view->music == GCU_MUSIC_PLAYING) {
    draw_text(16, mid_y + 60, GCU_PLAYING_CUE, ink, bg, 1);
  } else if (s_music_missing) {
    draw_text(16, mid_y + 60, "no first.pcm", ink, bg, 1);
  }

  draw_button_hints(view, th);
}

typedef struct {
  float ax, ay, az;
  float gx, gy, gz;
  float temp_c;
  int btn_a, btn_b, btn_c;
} imu_sample_t;

static esp_err_t mpu_write(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_write_to_device(I2C_PORT, MPU_ADDR, buf, 2,
                                    pdMS_TO_TICKS(50));
}

static esp_err_t mpu_read(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_write_read_device(I2C_PORT, MPU_ADDR, &reg, 1, data, len,
                                      pdMS_TO_TICKS(50));
}

static int read_imu(imu_sample_t *out) {
  uint8_t raw[14];
  if (!s_imu_ok || !out) {
    return 0;
  }
  if (mpu_read(MPU_ACCEL_XOUT_H, raw, 14) != ESP_OK) {
    return 0;
  }
  int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
  int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
  int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
  int16_t t = (int16_t)((raw[6] << 8) | raw[7]);
  int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
  int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);
  /* ±2g default scale 16384 LSB/g; ±250 dps 131 LSB/(°/s) after reset. */
  out->ax = ax / 16384.0f;
  out->ay = ay / 16384.0f;
  out->az = az / 16384.0f;
  out->gx = gx / 131.0f;
  out->gy = gy / 131.0f;
  out->gz = gz / 131.0f;
  /* MPU6886 temperature (not MPU6050 formula). */
  out->temp_c = (t / 326.8f) + 25.0f;
  out->btn_a = gpio_get_level(GCU_PIN_BTN_A) == 0;
  out->btn_b = gpio_get_level(GCU_PIN_BTN_B) == 0;
  out->btn_c = gpio_get_level(GCU_PIN_BTN_C) == 0;
  return 1;
}

static void draw_details_chrome(const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  char line[48];
  fill_rect(0, 0, GCU_DISPLAY_W, GCU_DISPLAY_H, bg);
  gcu_identity_line(line, (int)sizeof line);
  draw_text(8, 8, line, ink, bg, 1);
  draw_text(8, 28, "ax ay az (g)", ink, bg, 1);
  draw_text(8, 56, "gx gy gz (dps)", ink, bg, 1);
  draw_text(8, 84, "temp C", ink, bg, 1);
  draw_text(8, 112, "btns A B C", ink, bg, 1);
  draw_text(8, 140, "heap", ink, bg, 1);
  draw_text(8, GCU_DISPLAY_H - 20, "A=face  B=play  C=noop", ink, bg, 1);
}

static void draw_details_values(const gcu_theme_t *th) {
  uint16_t bg = rgb565(th->bg.r, th->bg.g, th->bg.b);
  uint16_t ink = rgb565(th->banner_ink.r, th->banner_ink.g, th->banner_ink.b);
  char line[56];
  imu_sample_t s;
  /* Value strips only (partial update). */
  fill_rect(8, 40, GCU_DISPLAY_W - 16, 12, bg);
  fill_rect(8, 68, GCU_DISPLAY_W - 16, 12, bg);
  fill_rect(8, 96, GCU_DISPLAY_W - 16, 12, bg);
  fill_rect(8, 124, GCU_DISPLAY_W - 16, 12, bg);
  fill_rect(8, 152, GCU_DISPLAY_W - 16, 12, bg);

  if (read_imu(&s)) {
    snprintf(line, sizeof line, "%+.2f %+.2f %+.2f", s.ax, s.ay, s.az);
    draw_text(8, 40, line, ink, bg, 1);
    snprintf(line, sizeof line, "%+.1f %+.1f %+.1f", s.gx, s.gy, s.gz);
    draw_text(8, 68, line, ink, bg, 1);
    snprintf(line, sizeof line, "%+.1f", s.temp_c);
    draw_text(8, 96, line, ink, bg, 1);
    snprintf(line, sizeof line, "%d %d %d", s.btn_a, s.btn_b, s.btn_c);
    draw_text(8, 124, line, ink, bg, 1);
  } else {
    draw_text(8, 40, "IMU not ready", ink, bg, 1);
  }
  snprintf(line, sizeof line, "%u", (unsigned)esp_get_free_heap_size());
  draw_text(8, 152, line, ink, bg, 1);
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

static void audio_soft_park(void) {
  uint8_t fade[256];
  if (!s_dac) {
    return;
  }
  memset(fade, 128, sizeof fade);
  /* portMAX_DELAY: APLL/DMA descriptor handoff can stall briefly under SPI load. */
  dac_continuous_write(s_dac, fade, sizeof fade, NULL, (TickType_t)portMAX_DELAY);
  memset(fade, 0, sizeof fade);
  dac_continuous_write(s_dac, fade, sizeof fade, NULL, (TickType_t)portMAX_DELAY);
}

/* Chunked PCM write so FreeRTOS can run UI/serial between DMA fills. */
static int play_pcm_chunked(const uint8_t *data, size_t len, int64_t *io_offset,
                            int allow_pause) {
  size_t off = io_offset ? (size_t)(*io_offset) : 0;
  if (off > len) {
    off = 0;
  }
  while (off < len) {
    if (allow_pause && s_audio_pause_req) {
      s_audio_pause_req = 0;
      if (io_offset) {
        *io_offset = (int64_t)off;
      }
      s_audio_offset = (int64_t)off;
      audio_soft_park();
      return 1; /* paused */
    }
    size_t n = len - off;
    if (n > AUDIO_CHUNK) {
      n = AUDIO_CHUNK;
    }
    size_t written = 0;
    esp_err_t err =
        dac_continuous_write(s_dac, (uint8_t *)(data + off), n, &written,
                             (TickType_t)portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "dac write: %s", esp_err_to_name(err));
      break;
    }
    off += written ? written : n;
    if (io_offset) {
      *io_offset = (int64_t)off;
    }
    s_audio_offset = (int64_t)off;
    /* Yield so UI/serial keep running while song plays. */
    taskYIELD();
  }
  return 0;
}

static int play_file_chunked(const char *path, int64_t start_off) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    ESP_LOGW(TAG, "open %s failed", path);
    s_music_missing = 1;
    return -1;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return -1;
  }
  long sz = ftell(f);
  if (sz <= 0) {
    fclose(f);
    s_music_missing = 1;
    return -1;
  }
  s_song_size = sz;
  s_music_missing = 0;
  if (start_off < 0) {
    start_off = 0;
  }
  if (start_off >= sz) {
    start_off = 0;
  }
  fseek(f, (long)start_off, SEEK_SET);

  uint8_t *buf = (uint8_t *)malloc(AUDIO_CHUNK);
  if (!buf) {
    fclose(f);
    return -1;
  }

  int64_t off = start_off;
  int paused = 0;
  while (off < sz) {
    if (s_audio_pause_req) {
      s_audio_pause_req = 0;
      s_audio_offset = off;
      paused = 1;
      audio_soft_park();
      break;
    }
    size_t want = AUDIO_CHUNK;
    if ((int64_t)want > sz - off) {
      want = (size_t)(sz - off);
    }
    size_t got = fread(buf, 1, want, f);
    if (got == 0) {
      break;
    }
    size_t written = 0;
    esp_err_t err = dac_continuous_write(s_dac, buf, got, &written,
                                         (TickType_t)portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "song dac: %s", esp_err_to_name(err));
      break;
    }
    off += (int64_t)(written ? written : got);
    s_audio_offset = off;
    if (written && written < got) {
      fseek(f, (long)off, SEEK_SET);
    }
    taskYIELD();
  }
  free(buf);
  fclose(f);
  if (!paused && off >= sz) {
    s_audio_offset = 0;
    audio_soft_park();
    s_music_ended = 1;
  }
  return paused ? 1 : 0;
}

static void audio_task(void *arg) {
  (void)arg;
  for (;;) {
    if (s_want_boot_riff && s_dac) {
      s_want_boot_riff = 0;
      s_audio_playing = 1;
      xSemaphoreTake(s_audio_mu, portMAX_DELAY);
      int64_t off = 0;
      play_pcm_chunked(BOOT_RIFF_PCM, BOOT_RIFF_PCM_LEN, &off, 0);
      audio_soft_park();
      xSemaphoreGive(s_audio_mu);
      s_audio_playing = 0;
    }

    if (s_want_full_play && s_dac) {
      s_want_full_play = 0;
      s_audio_playing = 1;
      int64_t start = s_audio_offset;
      xSemaphoreTake(s_audio_mu, portMAX_DELAY);
      if (s_spiffs_ok) {
        play_file_chunked(SONG_PATH, start);
      } else {
        s_music_missing = 1;
        s_music_ended = 1;
        ESP_LOGW(TAG, "SPIFFS missing — cannot play full track");
      }
      xSemaphoreGive(s_audio_mu);
      s_audio_playing = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
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
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
}

static void init_audio(void) {
  dac_continuous_config_t cont_cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0,
      .desc_num = 8,
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
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io);
}

static void init_imu(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = GCU_PIN_I2C_SDA,
      .scl_io_num = GCU_PIN_I2C_SCL,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 400000,
  };
  esp_err_t err = i2c_param_config(I2C_PORT, &conf);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_param_config: %s", esp_err_to_name(err));
    return;
  }
  err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2c_driver_install: %s", esp_err_to_name(err));
    return;
  }
  uint8_t who = 0;
  if (mpu_read(MPU_WHO_AM_I_REG, &who, 1) != ESP_OK || who != MPU_WHO_AM_I_VAL) {
    ESP_LOGW(TAG, "MPU WHO_AM_I=0x%02x (want 0x%02x)", who, MPU_WHO_AM_I_VAL);
    s_imu_ok = 0;
    return;
  }
  if (mpu_write(MPU_PWR_MGMT_1, 0x00) != ESP_OK) {
    s_imu_ok = 0;
    return;
  }
  s_imu_ok = 1;
  ESP_LOGI(TAG, "MPU6886 ready");
}

static void init_spiffs(void) {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 4,
      .format_if_mount_failed = false,
  };
  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "SPIFFS mount failed: %s (flash first.pcm via tools/flash_spiffs.ps1)",
             esp_err_to_name(err));
    s_spiffs_ok = 0;
    s_music_missing = 1;
    return;
  }
  FILE *f = fopen(SONG_PATH, "rb");
  if (!f) {
    ESP_LOGW(TAG, "%s missing on SPIFFS", SONG_PATH);
    s_spiffs_ok = 1; /* mounted, file absent */
    s_music_missing = 1;
    return;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fclose(f);
  if (sz <= 0) {
    s_music_missing = 1;
    s_spiffs_ok = 1;
    return;
  }
  s_song_size = sz;
  s_spiffs_ok = 1;
  s_music_missing = 0;
  ESP_LOGI(TAG, "first.pcm size=%ld", sz);
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
  (void)ensure_shadow();
  init_spiffs();
  init_leds();
  init_display();
  init_audio();
  init_buttons();
  init_imu();
  stdin_set_nonblocking();
  /* Higher priority than idle; lower than WiFi defaults — leave UI room. */
  xTaskCreate(audio_task, "audio", 6144, NULL, 4, NULL);
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
    /* Resume from domain offset if paused mid-track. */
    if (st) {
      s_audio_offset = st->music_offset_bytes;
    }
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
      draw_details_chrome(th);
      draw_details_values(th);
      if (st && st->hal && st->hal->now_ms) {
        s_last_details_ms = st->hal->now_ms(st->hal);
      }
    } else {
      draw_face_full(view, th);
    }
    return;
  }

  if (view->screen == GCU_SCREEN_DETAILS) {
    int64_t now = (st && st->hal && st->hal->now_ms) ? st->hal->now_ms(st->hal)
                                                     : 0;
    if (now - s_last_details_ms >= (int64_t)GCU_DEFAULTS.details_refresh_ms) {
      s_last_details_ms = now;
      draw_details_values(th);
    }
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
  /* Play/pause flips middle glyph without a full face clear. */
  if ((int)view->music != s_last_music_for_hints) {
    draw_button_hints(view, th);
  }
}

void board_poll_domain_events(gcu_state_t *st) {
  if (!st) {
    return;
  }
  if (s_music_ended) {
    s_music_ended = 0;
    gcu_on_music_ended(st);
  }
  /* Keep domain offset in sync while playing/paused. */
  if (st->music == GCU_MUSIC_PLAYING || st->music == GCU_MUSIC_PAUSED) {
    gcu_set_music_offset(st, s_audio_offset);
  }
}

void board_poll_buttons(gcu_state_t *st) {
  if (!st || !st->hal || !st->hal->now_ms) {
    return;
  }
  int64_t now = st->hal->now_ms(st->hal);
  const int pins[3] = {GCU_PIN_BTN_A, GCU_PIN_BTN_B, GCU_PIN_BTN_C};
  const gcu_btn_t btns[3] = {GCU_BTN_A, GCU_BTN_B, GCU_BTN_C};
  for (int i = 0; i < 3; i++) {
    int level = gpio_get_level(pins[i]);
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
  s_audio_pause_req = 1;
  s_want_full_play = 0;
  s_want_boot_riff = 0;
  if (s_strip) {
    led_strip_clear(s_strip);
  }
  if (s_dac && s_audio_mu) {
    uint8_t z[64];
    memset(z, 0, sizeof z);
    if (xSemaphoreTake(s_audio_mu, pdMS_TO_TICKS(200)) == pdTRUE) {
      dac_continuous_write(s_dac, z, sizeof z, NULL, pdMS_TO_TICKS(100));
      xSemaphoreGive(s_audio_mu);
    }
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
        } else if (strcmp(p, "shot") == 0 || strcmp(p, "frame") == 0) {
          /* Shadow framebuffer dump for host/agent vision (no camera). */
          board_send_shot();
        } else if (strcmp(p, "repl") == 0) {
          board_park_outputs();
          printf("ok repl parked\n");
          fflush(stdout);
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
