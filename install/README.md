# Install / update (Xuss-C)

## What good looks like (hello-metal product face)

After power-on or reset, on the M5GO:

1. Serial prints `fw_name=XUSSC fw_version=…` first (115200 baud).
2. A short, gentle tone (~0.8 s, 441 Hz) plays on the built-in speaker.
3. The side LED strips blink blue about twice per second, indefinitely.

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
then hard-resets — expect the boot tone again after the reset.
