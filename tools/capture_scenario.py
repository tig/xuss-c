#!/usr/bin/env python3
"""Drive Xuss-C over serial and assemble a scenario GIF.

Sequence (default):
  boot settle → color → gear → color → color → play → wait 5s → color
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

# Reuse shot helpers from sibling module
sys.path.insert(0, str(Path(__file__).resolve().parent))
from shot import grab_one  # noqa: E402

try:
    import serial
except ImportError as e:
    print("pyserial required", file=sys.stderr)
    raise SystemExit(2) from e

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as e:
    print("Pillow required", file=sys.stderr)
    raise SystemExit(2) from e


def wait_ok(ser: serial.Serial, prefix: str = "ok", timeout: float = 3.0) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("utf-8", errors="replace").strip()
        if text.startswith(prefix) or text.startswith("err"):
            return text
    return ""


def btn(ser: serial.Serial, which: str) -> None:
    ser.reset_input_buffer()
    ser.write(f"btn {which}\n".encode())
    ser.flush()
    wait_ok(ser, "ok btn", timeout=2.0)
    # Let main loop apply view + paint
    time.sleep(0.35)


def label_frame(img: Image.Image, caption: str) -> Image.Image:
    """Annotate a frame so the GIF is self-explanatory."""
    out = img.copy().convert("RGB")
    draw = ImageDraw.Draw(out)
    bar_h = 16
    draw.rectangle([0, 0, out.width, bar_h], fill=(0, 0, 0))
    draw.text((4, 2), caption[:48], fill=(255, 255, 255))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("-o", "--output", default="capture/scenario.gif")
    ap.add_argument("--boot-wait", type=float, default=4.0, help="seconds after open for boot face")
    ap.add_argument("--play-wait", type=float, default=5.0, help="seconds while music plays")
    ap.add_argument("--hold", type=float, default=0.4, help="extra settle after each btn")
    args = ap.parse_args()

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    stem = out.with_suffix("")

    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    frames: list[Image.Image] = []
    step = 0

    def snap(caption: str) -> None:
        nonlocal step
        img, _ = grab_one(ser, timeout_s=90.0)
        labeled = label_frame(img, caption)
        path = Path(f"{stem}_{step:02d}.png")
        labeled.save(path)
        frames.append(labeled)
        print(f"OK {path} — {caption}")
        step += 1

    try:
        ser.dtr = False
        ser.rts = False
        # Soft reboot so "starting" is in the timeline
        ser.reset_input_buffer()
        ser.write(b"reboot\n")
        ser.flush()
        time.sleep(0.5)
        ser.close()
        time.sleep(1.0)
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
        ser.dtr = False
        ser.rts = False
        print(f"boot wait {args.boot_wait}s…")
        time.sleep(args.boot_wait)
        ser.reset_input_buffer()

        snap("0 boot / idle face (blue)")
        btn(ser, "a")
        time.sleep(args.hold)
        snap("1 color once → next theme")
        btn(ser, "c")
        time.sleep(args.hold)
        snap("2 gear → Details")
        btn(ser, "a")
        time.sleep(args.hold)
        snap("3 color #1 after gear (exit Details)")
        btn(ser, "a")
        time.sleep(args.hold)
        snap("4 color #2 after gear (theme step)")
        btn(ser, "b")
        time.sleep(args.hold)
        snap("5 play")
        print(f"play wait {args.play_wait}s…")
        time.sleep(args.play_wait)
        snap(f"6 still playing (~{args.play_wait:.0f}s)")
        btn(ser, "a")
        time.sleep(args.hold)
        snap("7 color while playing → pause")

        # GIF: hold each step ~1.2s for readability
        frames[0].save(
            out,
            save_all=True,
            append_images=frames[1:],
            duration=1200,
            loop=0,
        )
        print(f"OK wrote {out} ({len(frames)} frames)")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
