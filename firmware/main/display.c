/* Device display path — allowlisted for esp_lcd / driver headers. */
#include "display.h"

#include "gcu/defaults.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "display";
static esp_lcd_panel_handle_t s_panel;
static int s_ready;
/* Half-res shadow for esprec (full 320x240 BSS overflows DRAM on classic ESP32). */
#define SHADOW_W (GCU_LCD_W / 2)
#define SHADOW_H (GCU_LCD_H / 2)
static uint16_t s_fb[SHADOW_W * SHADOW_H];

int display_width(void) { return SHADOW_W; }
int display_height(void) { return SHADOW_H; }
const uint16_t *display_framebuffer(void) { return s_ready ? s_fb : NULL; }

int display_init(void) {
  if (s_ready) {
    return 0;
  }

  gpio_config_t bk = {
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = 1ULL << GCU_PIN_LCD_BL,
  };
  ESP_ERROR_CHECK(gpio_config(&bk));
  gpio_set_level(GCU_PIN_LCD_BL, 1);

  spi_bus_config_t bus = {
      .sclk_io_num = GCU_PIN_LCD_SCLK,
      .mosi_io_num = GCU_PIN_LCD_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = GCU_LCD_W * 40 * sizeof(uint16_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io = NULL;
  esp_lcd_panel_io_spi_config_t io_cfg = {
      .cs_gpio_num = GCU_PIN_LCD_CS,
      .dc_gpio_num = GCU_PIN_LCD_DC,
      .spi_mode = 0,
      .pclk_hz = 26 * 1000 * 1000,
      .trans_queue_depth = 10,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST,
                                          &io_cfg, &io));

  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = GCU_PIN_LCD_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_cfg, &s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
  /* MADCTL BGR-only + invert — measured M5GO path (knowledge/esp32-lcd-ips). */
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

  s_ready = 1;
  ESP_LOGI(TAG, "ILI9342C ready %dx%d", GCU_LCD_W, GCU_LCD_H);
  return 0;
}

/* SPI bulk blit wants big-endian RGB565 words (knowledge/esp32-lcd-ips). */
static inline uint16_t rgb565_spi_be(uint16_t native) {
  return (uint16_t)((native >> 8) | (native << 8));
}

void display_blit(int x, int y, int w, int h, const uint16_t *pixels_native) {
  if (!s_ready || !s_panel || !pixels_native || w <= 0 || h <= 0) {
    return;
  }
  if (x < 0 || y < 0 || x + w > GCU_LCD_W || y + h > GCU_LCD_H) {
    return;
  }
  size_t n = (size_t)w * (size_t)h;
  uint16_t *wire = (uint16_t *)malloc(n * sizeof(uint16_t));
  if (!wire) {
    return;
  }
  for (size_t i = 0; i < n; i++) {
    wire[i] = rgb565_spi_be(pixels_native[i]);
  }
  /* Update half-res shadow (nearest). */
  for (int row = 0; row < h; row += 2) {
    for (int col = 0; col < w; col += 2) {
      int sx = (x + col) / 2;
      int sy = (y + row) / 2;
      if (sx >= 0 && sx < SHADOW_W && sy >= 0 && sy < SHADOW_H) {
        s_fb[sy * SHADOW_W + sx] = wire[row * w + col];
      }
    }
  }
  esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, wire);
  free(wire);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t rgb565) {
  if (!s_ready || !s_panel || w <= 0 || h <= 0) {
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
  if (x + w > GCU_LCD_W) {
    w = GCU_LCD_W - x;
  }
  if (y + h > GCU_LCD_H) {
    h = GCU_LCD_H - y;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  const uint16_t wire = rgb565_spi_be(rgb565);

  /* Half-res shadow stores spi_be words so esprec pack=spi_be matches glass. */
  if (s_ready) {
    int x0 = x / 2;
    int y0 = y / 2;
    int x1 = (x + w + 1) / 2;
    int y1 = (y + h + 1) / 2;
    if (x0 < 0) {
      x0 = 0;
    }
    if (y0 < 0) {
      y0 = 0;
    }
    if (x1 > SHADOW_W) {
      x1 = SHADOW_W;
    }
    if (y1 > SHADOW_H) {
      y1 = SHADOW_H;
    }
    for (int sy = y0; sy < y1; sy++) {
      for (int sx = x0; sx < x1; sx++) {
        s_fb[sy * SHADOW_W + sx] = wire;
      }
    }
  }

  /* Chunked solid fill to keep stack/heap modest. */
  const int chunk_h = 8;
  size_t row_bytes = (size_t)w * sizeof(uint16_t);
  uint16_t *buf = (uint16_t *)malloc(row_bytes * (size_t)chunk_h);
  if (!buf) {
    return;
  }
  for (int i = 0; i < w * chunk_h; i++) {
    buf[i] = wire;
  }
  for (int row = 0; row < h; row += chunk_h) {
    int hh = h - row;
    if (hh > chunk_h) {
      hh = chunk_h;
    }
    if (hh < chunk_h) {
      for (int i = 0; i < w * hh; i++) {
        buf[i] = wire;
      }
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + hh, buf);
  }
  free(buf);
}
