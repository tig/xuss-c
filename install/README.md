# Install / update (Xuss-C)

## Host gate

On Windows, activate ESP-IDF for `cmake`/`ninja`, then prefer a **host** MinGW `gcc` (not IDF `esp-clang`) for the pure-C host tests:

```text
cmake -S host -B build/host -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build/host --target host_test
silico gate
silico product-path
```

## Deploy

After operator confirms board identity (or pre-approves a port):

```text
silico inspect --port COMx
silico deploy --port COMx --yes --verify --reset
# once per machine / after partition change: full-track asset
# (activate ESP-IDF first; needs assets/first.pcm)
powershell -File tools/flash_spiffs.ps1 -Port COMx
```

Requires ESP-IDF (`idf.py` or `IDF_PATH`). First flash and app update use the same image path. Flash size on M5GO is **16MB**. Custom partition table: 2MB app + SPIFFS storage for `first.pcm`.

## Product face “good” (operator)

After soft-reset / power-on:

1. Serial identity: `fw_name=XUSSC fw_version=0.0.1` (boot print + `identity` knock).
2. Short boot riff (~2.5 s excerpt of *First*) on the speaker.
3. Blue living face on the IPS: eyes, smile, scrolling banner `Xuss-C; built on ESP-IDF`, bottom hints.
4. Side LED strips in the face color; right-eye wink about every 10 s.
5. Buttons: **A** cycles theme (or pauses while playing without theme change), **B** plays/pauses full *First* from SPIFFS with **First by Tig** cue, **C** opens Details (IMU values ~10 Hz; music continues).

## Escape hatch

USB ASCII lines: `identity`, `repl` (park outputs), `reboot`.
