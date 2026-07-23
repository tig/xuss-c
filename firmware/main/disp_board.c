/* Device display backend — allowlisted for device headers.
 * M5Stack Core: ILI9342C 320x240 IPS on SPI3 (SCLK=18, MOSI=23, CS=14,
 * DC=27, RST=33, backlight GPIO=32). ILI9342C is driven by the ili9341
 * driver with color inversion on (M5 panels). */
#include "disp_board.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

#define PIN_SCLK 18
#define PIN_MOSI 23
#define PIN_CS 14
#define PIN_DC 27
#define PIN_RST 33
#define PIN_BL 32

#define BLIT_MAX_PX (320 * 48)

static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t done_sem;
static uint16_t *bounce; /* DMA-capable, byte-swapped copy for the panel */

static bool on_done(esp_lcd_panel_io_handle_t io,
                    esp_lcd_panel_io_event_data_t *edata, void *user) {
  (void)io;
  (void)edata;
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR((SemaphoreHandle_t)user, &woken);
  return woken == pdTRUE;
}

int gcu_board_disp_init(void) {
  spi_bus_config_t bus = {
      .sclk_io_num = PIN_SCLK,
      .mosi_io_num = PIN_MOSI,
      .miso_io_num = -1,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = BLIT_MAX_PX * 2,
  };
  if (spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
    return 0;
  }

  done_sem = xSemaphoreCreateBinary();
  esp_lcd_panel_io_handle_t io = NULL;
  esp_lcd_panel_io_spi_config_t io_cfg = {
      .dc_gpio_num = PIN_DC,
      .cs_gpio_num = PIN_CS,
      .pclk_hz = 40 * 1000 * 1000,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .spi_mode = 0,
      .trans_queue_depth = 4,
      .on_color_trans_done = on_done,
      .user_ctx = done_sem,
  };
  if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_cfg,
                               &io) != ESP_OK) {
    return 0;
  }

  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = PIN_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  if (esp_lcd_new_panel_ili9341(io, &panel_cfg, &panel) != ESP_OK) {
    return 0;
  }
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_panel_invert_color(panel, true); /* M5 ILI9342C */
  esp_lcd_panel_disp_on_off(panel, true);

  bounce = heap_caps_malloc(BLIT_MAX_PX * 2, MALLOC_CAP_DMA);
  if (!bounce) {
    return 0;
  }

  gpio_reset_pin(PIN_BL);
  gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_BL, 1); /* backlight on */
  return 1;
}

void gcu_board_disp_blit(int x, int y, int w, int h,
                         const unsigned short *px) {
  if (!panel || !bounce || w <= 0 || h <= 0 || w * h > BLIT_MAX_PX) {
    return;
  }
  int n = w * h;
  for (int i = 0; i < n; i++) {
    bounce[i] = (uint16_t)((px[i] << 8) | (px[i] >> 8)); /* SPI big-endian */
  }
  if (esp_lcd_panel_draw_bitmap(panel, x, y, x + w, y + h, bounce) == ESP_OK) {
    xSemaphoreTake(done_sem, pdMS_TO_TICKS(500)); /* bounce reusable after */
  }
}
