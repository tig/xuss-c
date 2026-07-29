idf_component_register(
  SRCS "main.c" "hal_board.c" "display.c" "leds.c" "audio.c"
       "../../src/defaults.c" "../../src/domain.c"
  INCLUDE_DIRS "." "../../include"
  EMBED_FILES "../../assets/boot_riff.pcm"
)
