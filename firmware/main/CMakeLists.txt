idf_component_register(
  SRCS "main.c" "hal_board.c"
       "../../src/defaults.c" "../../src/domain.c"
       "../../src/themes.c" "../../src/music.c" "../../src/face.c"
       "../../src/app.c" "../../src/protocol.c" "../../src/details.c"
  INCLUDE_DIRS "." "../../include"
)
