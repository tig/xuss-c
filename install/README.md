# Xuss-C install / update path

## Host gate (this computer)

```text
cmake -S host -B build/host -G Ninja
cmake --build build/host --target host_test
silico gate
```

## Flash (after operator confirms board identity)

```text
# Activate ESP-IDF, then:
silico deploy --port COMx --yes --verify --reset
```

Identity on the link: `fw_name=XUSSC fw_version=0.2.0` (product contract).

Flash needs **16MB** partition map (`firmware/partitions.csv`) and SPIFFS with `first.pcm`:

```text
# After app flash (silico deploy / idf.py flash):
py -3 tools/gen_spiffs_image.py --idf-path %IDF_PATH% --assets assets --out build/spiffs.bin --size 0xE70000
python -m esptool --chip esp32 -p COMx -b 460800 write_flash --flash_size 16MB 0x190000 build/spiffs.bin
```

## Product face “good” (metal acceptance)

After boot you should:

1. **Hear** a short multi-note boot greeting (~2.5s) from the M5GO speaker.
2. **See** a living face on the IPS: round eyes + arc smile, eyebrows, scrolling banner  
   `Xuss-C; built on ESP-IDF` (seamless shift), dark-blue default theme.
3. Side LED strips match the active theme color.
4. Right eye **winks** about every 10 seconds.
5. Front glyphs: **A** theme chips, **B** play/pause (streams full *First* from SPIFFS when present), **C** gear → Details.
6. **Details**: live IMU accel/gyro/temp, button states, heap; music can keep playing.
7. Serial identity: `fw_name=XUSSC fw_version=0.2.0` (and `shot` for esprec capture).
