idf_component_register(
  SRCS "main.c" "hal_board.c"
       "../../src/defaults.c" "../../src/domain.c"
  INCLUDE_DIRS "." "../../include"
  REQUIRES driver esp_lcd esp_timer freertos esp_driver_dac esp_driver_gpio
           esp_driver_spi esp_driver_i2c esp_driver_uart spiffs esprec
)
