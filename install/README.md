# Install / update (Xuss-C)

## What good looks like (hello-metal product face)

After power-on or reset, on the M5GO:

1. Serial prints `fw_name=XUSSC fw_version=…` first (115200 baud).
2. The boot riff plays — a ~2.5 s recognizable slice of *First* by Tig.
3. Serial prints the song status: `audio=ok bytes=… rate=22050`
   (or `audio=missing` if the song partition was never flashed).
4. The side LED strips blink blue about twice per second, indefinitely.

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
