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

Identity on the link: `fw_name=XUSSC fw_version=0.1.0` (product contract).

## Product face “good” (metal acceptance)

After boot you should:

1. **Hear** a short multi-note boot greeting (~2.5s) from the M5GO speaker.
2. **See** a living face on the IPS: eyes + smile, scrolling banner  
   `Xuss-C; built on ESP-IDF`, theme-colored side LED strips.
3. Right eye **winks** about every 10 seconds.
4. Front buttons: **A** cycles color themes, **B** replays the short riff (stand-in until full *First* PCM is product-sourced), **C** opens a Details stub.

Full-track *First by Tig* and live IMU Details are the next domain slice.
