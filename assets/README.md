# Assets

`boot-riff.u8.raw` is the boot greeting: seconds 7.5 to 10 of *First* (Tig Kindel, 2012, an ACID composition; maker’s own work). Format: 11,025 Hz, 8-bit unsigned, mono, 200 ms fade-out, 27563 bytes. Played once at boot via the ESP32 DAC.

Product audio is sample PCM only (boot riff + full *First*). Prefer regenerating full-track assets at the project sample rate in `spec.md` (22,050 Hz unsigned 8-bit mono preferred for Xuss-C) when deploying the full song.

Regenerate the boot riff from the master (11,025 Hz legacy slice, if keeping this file as-is):

```text
ffmpeg -ss 7.5 -t 2.5 -i first.mp3 -ac 1 -ar 11025 -af "afade=t=out:st=2.3:d=0.2" -f u8 boot-riff.u8.raw
```

For a 22,050 Hz boot riff aligned with the preferred full-track rate:

```text
ffmpeg -ss 7.5 -t 2.5 -i first.mp3 -ac 1 -ar 22050 -af "afade=t=out:st=2.3:d=0.2" -f u8 boot-riff.u8.raw
```
