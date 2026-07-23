# Install / update (Xuss-C)

## What good looks like (product face)

After power-on or reset, on the M5GO:

1. Serial prints `fw_name=XUSSC fw_version=…` first (115200 baud),
   then `audio=ok bytes=… rate=22050`, `link=ok`, `display=ok`, `imu=ok`.
2. The boot riff plays — a ~2.5 s recognizable slice of *First* by Tig,
   easing out (no hard cut).
3. The screen shows the idle face: blue theme, scrolling banner
   `Xuss-C; built on ESP-IDF`, eyes + smile, button hints
   (color / play triangle / gear). The right eye winks every ~10 s.
   Side LED strips glow the theme color.
4. Buttons per the manual: A cycles blue→orange→red→green→black
   (black = white background, side LEDs off); B plays / pauses / resumes
   the full *First* track while the face keeps living; C opens Details
   (live IMU values ~10x/s, works while music plays; A exits without a
   theme change).
5. The serial link answers `identity`, `repl` (parks outputs,
   flashable state), and `reboot` — including mid-song.

No tone or no side lights after a good flash = product defect; see
`firmware/main/hal_board.c` (pins come from `include/gcu/defaults.h`:
strip GPIO15, speaker DAC GPIO25).

The full spec.md product face (screen face, boot riff from *First*,
buttons, Details) is later domain work — not yet in this build.

## Host gate

```text
cmake -S host -B build/host
cmake --build build/host --target host_test
```

## Update path

Requires ESP-IDF v5.3.2 (`idf.py` on PATH or `IDF_PATH` set). First flash
and app update use the same image path. After the operator confirms board
identity (which COM port is the M5GO):

```text
silico deploy --port COMx --yes
```

This rebuilds with `idf.py` and overwrites the whole application image,
then hard-resets — expect the boot riff again after the reset.

## Song data (audio partition)

The full track streams from the `audio` flash partition (never RAM).
Regenerate the blob from `assets/First.mp3` and write it once (or after
changing the track):

```text
python tools/make_audio_assets.py
python -m esptool --chip esp32 -p COMx -b 460800 write_flash 0x210000 build/first_audio.bin
```

The offset matches `firmware/partitions.csv` (`audio` at 0x210000).
Firmware validates the 16-byte XCA1 header; a bad/absent blob reports
`audio=missing` and playback is refused while the UI stays usable.
