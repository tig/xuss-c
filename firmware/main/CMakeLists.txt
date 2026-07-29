idf_component_register(
  SRCS "main.c" "hal_board.c" "display.c" "leds.c" "audio.c"
       "../../src/defaults.c" "../../src/domain.c"
  INCLUDE_DIRS "." "../../include"
  EMBED_FILES "../../assets/boot_riff.pcm"
  REQUIRES esprec driver esp_timer esp_lcd freertos
  PRIV_REQUIRES espressif__esp_lcd_ili9341 espressif__led_strip
)
