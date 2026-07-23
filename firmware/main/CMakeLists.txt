idf_component_register(
  SRCS "main.c" "hal_board.c" "audio_board.c" "app_board.c" "disp_board.c"
       "../../src/defaults.c" "../../src/domain.c"
       "../../src/proto.c" "../../src/ui.c" "../../src/audio.c"
       "../../src/input.c" "../../src/face.c"
  INCLUDE_DIRS "." "../../include"
  EMBED_FILES "assets/riff.pcm"
)
