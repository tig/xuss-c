# Firmware (ESP-IDF)

Metal application lives here. Not yet scaffolded to a full IDF project.

Before implementing the **voice** sink, investigate:

https://github.com/The-Shreyas-M/ESP32-Synth

See `spec.md` §3.1. Record adopt/adapt/reject in the PR ambiguity log.

Planned layout (when cut):

```text
firmware/
  CMakeLists.txt      # idf_component / project
  main/
    main.c
    board_m5go.c
    ...
```

Host-pure logic stays in `src/` + `include/xuss/` so CTest does not need IDF.
