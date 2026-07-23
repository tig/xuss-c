idf_component_register(
  SRCS "main.c" "hal_board.c" "audio_board.c"
       "../../src/defaults.c" "../../src/domain.c"
       "../../src/proto.c" "../../src/ui.c" "../../src/audio.c"
  INCLUDE_DIRS "." "../../include"
  EMBED_FILES "assets/riff.pcm"
)
