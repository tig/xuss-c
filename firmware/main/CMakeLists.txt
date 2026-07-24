idf_component_register(
  SRCS "main.c" "hal_board.c" "hal_display.c" "hal_input.c" "hal_imu.c" "hal_leds.c"
       "../../src/defaults.c" "../../src/domain.c"
       "../../src/themes.c" "../../src/music.c" "../../src/face.c"
       "../../src/app.c" "../../src/protocol.c" "../../src/details.c"
       "../../src/render.c" "../../src/input.c"
  INCLUDE_DIRS "." "../../include"
  EMBED_FILES "first_riff.u8"
)

# Full-song asset: build a SPIFFS image from firmware/spiffs/ into the "storage"
# partition and flash it with the project. Generate the payload first with
# firmware/tools/make_song.sh (the .u8 is large and .gitignored).
spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../spiffs" FLASH_IN_PROJECT)
