/* M5GO ILI9342C IPS panel backend — device headers allowed here (hal_display).
 *
 * M5Stack Core / M5GO v2.7 pin map (measured-then-fixed, spec §6):
 *   SPI: MOSI=23 SCLK=18 CS=14 DC=27 RST=33  backlight BL=32
 *   320x240, BGR order, IPS needs display-inversion ON.
 */
#include "hal_display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "font8x8_basic.h" /* public-domain 8x8 font (see file header) */

#define PIN_MOSI 23
#define PIN_SCLK 18
#define PIN_CS 14
#define PIN_DC 27
#define PIN_RST 33
#define PIN_BL 32

#define LCD_W 320
#define LCD_H 240
#define CHUNK_PX 1024

static const char *TAG = "hal_display";
static spi_device_handle_t s_spi;
static uint16_t s_chunk[CHUNK_PX];

static void lcd_cmd(uint8_t cmd) {
  gpio_set_level(PIN_DC, 0);
  spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
  spi_device_polling_transmit(s_spi, &t);
}

static void lcd_data(const uint8_t *data, int len) {
  if (len <= 0) {
    return;
  }
  gpio_set_level(PIN_DC, 1);
  spi_transaction_t t = {.length = (size_t)len * 8, .tx_buffer = data};
  spi_device_polling_transmit(s_spi, &t);
}

static void lcd_data1(uint8_t d) { lcd_data(&d, 1); }

static uint16_t rgb565_be(gcu_rgb_t c) {
  uint16_t v = (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) |
                          (c.b >> 3));
  return (uint16_t)((v >> 8) | (v << 8)); /* big-endian for the panel */
}

static void set_window(int x0, int y0, int x1, int y1) {
  uint8_t buf[4];
  lcd_cmd(0x2A);
  buf[0] = x0 >> 8;
  buf[1] = x0 & 0xFF;
  buf[2] = x1 >> 8;
  buf[3] = x1 & 0xFF;
  lcd_data(buf, 4);
  lcd_cmd(0x2B);
  buf[0] = y0 >> 8;
  buf[1] = y0 & 0xFF;
  buf[2] = y1 >> 8;
  buf[3] = y1 & 0xFF;
  lcd_data(buf, 4);
  lcd_cmd(0x2C);
}

static void push_color(uint16_t be, long count) {
  for (int i = 0; i < CHUNK_PX; i++) {
    s_chunk[i] = be;
  }
  while (count > 0) {
    int n = count > CHUNK_PX ? CHUNK_PX : (int)count;
    lcd_data((const uint8_t *)s_chunk, n * 2);
    count -= n;
  }
}

static void clamp_rect(int *x, int *y, int *w, int *h) {
  if (*x < 0) {
    *w += *x;
    *x = 0;
  }
  if (*y < 0) {
    *h += *y;
    *y = 0;
  }
  if (*x + *w > LCD_W) {
    *w = LCD_W - *x;
  }
  if (*y + *h > LCD_H) {
    *h = LCD_H - *y;
  }
}

static void disp_fill_rect(gcu_gfx_t *self, int x, int y, int w, int h,
                           gcu_rgb_t c) {
  (void)self;
  clamp_rect(&x, &y, &w, &h);
  if (w <= 0 || h <= 0) {
    return;
  }
  set_window(x, y, x + w - 1, y + h - 1);
  push_color(rgb565_be(c), (long)w * h);
}

/* Draw one 8x8 glyph scaled, clipped to the panel, into a row buffer. */
static void disp_glyph(int x, int y, unsigned char ch, int scale, uint16_t fg,
                       uint16_t bg) {
  if (ch >= 128) {
    ch = '?';
  }
  const char *g = font8x8_basic[ch];
  int cw = 8 * scale;
  static uint16_t row[8 * 4];
  for (int gy = 0; gy < 8; gy++) {
    unsigned char bits = (unsigned char)g[gy];
    for (int gx = 0; gx < 8; gx++) {
      uint16_t col = (bits >> gx) & 1 ? fg : bg;
      for (int sx = 0; sx < scale; sx++) {
        row[gx * scale + sx] = col;
      }
    }
    /* clip this scaled row-band against the panel */
    int py = y + gy * scale;
    for (int sy = 0; sy < scale; sy++) {
      int yy = py + sy;
      if (yy < 0 || yy >= LCD_H) {
        continue;
      }
      int rx = x, rw = cw, off = 0;
      if (rx < 0) {
        off = -rx;
        rw += rx;
        rx = 0;
      }
      if (rx + rw > LCD_W) {
        rw = LCD_W - rx;
      }
      if (rw <= 0) {
        continue;
      }
      set_window(rx, yy, rx + rw - 1, yy);
      lcd_data((const uint8_t *)(row + off), rw * 2);
    }
  }
}

static void disp_text(gcu_gfx_t *self, int x, int y, const char *s, int scale,
                      gcu_rgb_t fg, gcu_rgb_t bg) {
  (void)self;
  if (!s || scale < 1) {
    return;
  }
  uint16_t f = rgb565_be(fg), b = rgb565_be(bg);
  int cw = 8 * scale;
  for (const char *p = s; *p; p++) {
    if (x >= LCD_W) {
      break;
    }
    if (x + cw > 0) {
      disp_glyph(x, y, (unsigned char)*p, scale, f, b);
    }
    x += cw;
  }
}

static gcu_gfx_t s_gfx = {
    .width = LCD_W,
    .height = LCD_H,
    .fill_rect = disp_fill_rect,
    .text = disp_text,
};

static void panel_init(void) {
  /* M5Stack ILI9342C init (from M5GFX Panel_ILI9342). ILI9342C is natively
   * 320x240 landscape, so no MV — MADCTL keeps MV=0 (0x08 = BGR). */
  lcd_cmd(0x01); /* SWRESET */
  vTaskDelay(pdMS_TO_TICKS(120));

  static const uint8_t setextc[] = {0xFF, 0x93, 0x42};
  lcd_cmd(0xC8);
  lcd_data(setextc, sizeof setextc);
  lcd_cmd(0xC0);
  lcd_data1(0x12);
  lcd_data1(0x12);
  lcd_cmd(0xC1);
  lcd_data1(0x03);
  lcd_cmd(0xC5);
  lcd_data1(0xF2);
  lcd_cmd(0xB0);
  lcd_data1(0xE0);
  static const uint8_t f6[] = {0x01, 0x00, 0x00};
  lcd_cmd(0xF6);
  lcd_data(f6, sizeof f6);
  static const uint8_t gmp[] = {0x00, 0x0C, 0x11, 0x04, 0x11, 0x08, 0x37, 0x89,
                                0x4C, 0x06, 0x0C, 0x0A, 0x2E, 0x34, 0x0F};
  lcd_cmd(0xE0);
  lcd_data(gmp, sizeof gmp);
  static const uint8_t gmn[] = {0x00, 0x0B, 0x11, 0x05, 0x13, 0x09, 0x33, 0x67,
                                0x48, 0x07, 0x0E, 0x0B, 0x2E, 0x33, 0x0F};
  lcd_cmd(0xE1);
  lcd_data(gmn, sizeof gmn);
  static const uint8_t dfun[] = {0x08, 0x82, 0x1D, 0x04};
  lcd_cmd(0xB6);
  lcd_data(dfun, sizeof dfun);

  lcd_cmd(0x36); /* MADCTL: BGR, native landscape (no MV) */
  lcd_data1(0x08);
  lcd_cmd(0x3A); /* 16-bit color */
  lcd_data1(0x55);
  lcd_cmd(0x21); /* INVON — IPS panel */
  lcd_cmd(0x11); /* SLPOUT */
  vTaskDelay(pdMS_TO_TICKS(120));
  lcd_cmd(0x29); /* DISPON */
  vTaskDelay(pdMS_TO_TICKS(20));
}

gcu_gfx_t *gcu_make_display(void) {
  gpio_config_t io = {
      .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST) | (1ULL << PIN_BL),
      .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io);

  gpio_set_level(PIN_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(PIN_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(20));

  spi_bus_config_t bus = {
      .mosi_io_num = PIN_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = PIN_SCLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = CHUNK_PX * 2 + 16,
  };
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
    ESP_LOGE(TAG, "spi bus init failed");
    return NULL;
  }
  spi_device_interface_config_t dev = {
      .clock_speed_hz = SPI_MASTER_FREQ_26M, /* 26.7MHz: safe write-only rate */
      .mode = 0,
      .spics_io_num = PIN_CS,
      .queue_size = 7,
      .flags = SPI_DEVICE_NO_DUMMY, /* write-only panel; skip RX timing check */
  };
  if (spi_bus_add_device(SPI2_HOST, &dev, &s_spi) != ESP_OK) {
    ESP_LOGE(TAG, "spi add device failed");
    return NULL;
  }

  panel_init();
  gpio_set_level(PIN_BL, 1); /* backlight on */
  return &s_gfx;
}
