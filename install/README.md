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
2. **See** a living face on the IPS: round eyes + arc smile, eyebrows, scrolling banner  
   `Xuss-C; built on ESP-IDF` (single-blit seamless shift), dark-blue default theme.
3. Side LED strips match the active theme color.
4. Right eye **winks** about every 10 seconds.
5. Front glyphs: **A** theme chips, **B** play triangle (pause bars while playing), **C** gear.
6. Serial identity: `fw_name=XUSSC fw_version=0.1.0` (and `shot` for esprec capture).

Full-track *First by Tig* PCM asset + live IMU Details are the next domain slice (Stage F).
