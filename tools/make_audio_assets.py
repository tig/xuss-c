#!/usr/bin/env python3
"""Build Xuss-C audio assets from assets/First.mp3 (requires ffmpeg).

Outputs:
  firmware/main/assets/riff.pcm  — boot riff, track seconds 7.5..10,
                                   unsigned 8-bit mono PCM @ 22050 Hz
                                   (committed; embedded into the app image)
  build/first_audio.bin          — 16-byte header + full-track PCM for the
                                   'audio' flash partition (generated at
                                   deploy time; not committed)

Header: b'XCA1' + u32le sample_rate + u32le pcm_byte_length + 4 zero bytes.
Firmware refuses playback when magic or length is bad (spec.md §4.4).
"""

import struct
import subprocess
import sys
from pathlib import Path

RATE = 22050
RIFF_START = 7.5
RIFF_END = 10.0
# Riff is a mid-track slice: ease in and out so it does not hard-cut
# (spec.md §4.1; operator: "ends sharply" without this).
RIFF_FADE_IN = 0.06
RIFF_FADE_OUT = 0.35
MAGIC = b"XCA1"

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "First.mp3"
RIFF_OUT = ROOT / "firmware" / "main" / "assets" / "riff.pcm"
FULL_OUT = ROOT / "build" / "first_audio.bin"


def ffmpeg_pcm(pre: list[str], post: list[str]) -> bytes:
    cmd = ["ffmpeg", "-v", "error", *pre, "-i", str(SRC), *post,
           "-ac", "1", "-ar", str(RATE), "-f", "u8", "-"]
    return subprocess.run(cmd, check=True, capture_output=True).stdout


def main() -> int:
    if not SRC.is_file():
        print(f"missing {SRC}", file=sys.stderr)
        return 1

    dur = RIFF_END - RIFF_START
    fade = (f"afade=t=in:st=0:d={RIFF_FADE_IN},"
            f"afade=t=out:st={dur - RIFF_FADE_OUT}:d={RIFF_FADE_OUT}")
    riff = ffmpeg_pcm(["-ss", str(RIFF_START), "-to", str(RIFF_END)],
                      ["-af", fade])
    RIFF_OUT.parent.mkdir(parents=True, exist_ok=True)
    RIFF_OUT.write_bytes(riff)
    print(f"riff: {len(riff)} bytes -> {RIFF_OUT}")

    full = ffmpeg_pcm([], [])
    FULL_OUT.parent.mkdir(parents=True, exist_ok=True)
    header = MAGIC + struct.pack("<II", RATE, len(full)) + b"\0" * 4
    FULL_OUT.write_bytes(header + full)
    print(f"full: {len(full)} bytes PCM (+16 header) -> {FULL_OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
