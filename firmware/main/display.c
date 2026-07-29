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

  /* Chunked solid fill to keep stack/heap modest. */
  const int chunk_h = 8;
  size_t row_bytes = (size_t)w * sizeof(uint16_t);
  uint16_t *buf = (uint16_t *)malloc(row_bytes * (size_t)chunk_h);
  if (!buf) {
    return;
  }
  for (int i = 0; i < w * chunk_h; i++) {
    buf[i] = rgb565;
  }
  for (int row = 0; row < h; row += chunk_h) {
    int hh = h - row;
    if (hh > chunk_h) {
      hh = chunk_h;
    }
    if (hh < chunk_h) {
      for (int i = 0; i < w * hh; i++) {
        buf[i] = rgb565;
      }
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y + row, x + w, y + row + hh, buf);
  }
  free(buf);
}
