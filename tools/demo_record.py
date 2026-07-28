#!/usr/bin/env python3
"""End-to-end Xuss-C product demo → smooth keyframe GIF via esprec.

Capture model is **host-paced single shot** (~18.5 s / full panel @ 115200).
Playback smoothness comes from GIF frame delays + settled keyframes, not from
high capture FPS. See ``tools/bench_capture_rate.py``.

Captions (if enabled) are drawn *above* the panel only.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from PIL import Image

try:
    from esprec.capture import capture_image
    from esprec.image_out import caption_above, save_gif, save_png
    from esprec.serial_port import open_port
except ImportError:
    print("pip install -e ../esprec", file=sys.stderr)
    raise SystemExit(2)

# Playback delays (ms) — tuned for readable state changes, not capture wall time.
DELAY_STATE_MS = 1100
DELAY_LIVING_MS = 400
DELAY_PLAY_MS = 1400
DELAY_DETAILS_MS = 1300


def wait_ok(ser, prefix: str = "ok", timeout: float = 3.0) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("utf-8", errors="replace").strip()
        if text.startswith(prefix) or text.startswith("err"):
            return text
    return ""


def btn(ser, which: str, hold: float = 0.5) -> None:
    ser.reset_input_buffer()
    ser.write(f"btn {which}\n".encode())
    ser.flush()
    wait_ok(ser, "ok btn", timeout=2.0)
    time.sleep(hold)


def snap(
    ser,
    path: Path,
    note: str,
    *,
    timeout: float,
    delay_ms: int,
    caption: bool,
) -> tuple[Image.Image, int]:
    meta, img = capture_image(ser, command="shot", timeout_s=timeout)
    rgb = img.convert("RGB")
    save_png(rgb, path)
    print(f"OK {path.name} {meta.w}x{meta.h} delay={delay_ms}ms — {note}")
    if caption:
        return caption_above(rgb, note), delay_ms
    return rgb, delay_ms


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("-o", "--outdir", default="docs/demo")
    ap.add_argument("--boot-wait", type=float, default=4.5)
    ap.add_argument("--play-wait", type=float, default=4.0)
    ap.add_argument("--hold", type=float, default=0.55, help="UI settle after btn")
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument(
        "--living-samples",
        type=int,
        default=3,
        help="back-to-back idle samples to show banner/wink change (serial-bound)",
    )
    ap.add_argument(
        "--captions",
        action="store_true",
        help="pad captions above panel (never over product chrome)",
    )
    ap.add_argument("--no-reboot", action="store_true")
    args = ap.parse_args()

    out = Path(args.outdir)
    out.mkdir(parents=True, exist_ok=True)

    frames: list[Image.Image] = []
    delays: list[int] = []
    step = 0

    def add(note: str, delay_ms: int, *, after_btn: str | None = None) -> None:
        nonlocal step
        if after_btn:
            btn(ser, after_btn, hold=args.hold)
        safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in note)[:48]
        path = out / f"{step:02d}_{safe}.png"
        img, d = snap(
            ser,
            path,
            note,
            timeout=args.timeout,
            delay_ms=delay_ms,
            caption=args.captions,
        )
        frames.append(img)
        delays.append(d)
        step += 1

    ser = open_port(args.port, args.baud)
    try:
        if not args.no_reboot:
            ser.reset_input_buffer()
            ser.write(b"reboot\n")
            ser.flush()
            time.sleep(0.4)
            ser.close()
            time.sleep(1.2)
            ser = open_port(args.port, args.baud)

        print(f"boot wait {args.boot_wait}s…")
        time.sleep(args.boot_wait)
        ser.reset_input_buffer()

        # Living face samples first (banner progresses between 18s dumps).
        n_live = max(1, args.living_samples)
        for i in range(n_live):
            add(f"idle living sample {i + 1}/{n_live}", DELAY_LIVING_MS)

        add("theme orange (A)", DELAY_STATE_MS, after_btn="a")
        add("Details sensors (C)", DELAY_DETAILS_MS, after_btn="c")
        add("exit Details face (A)", DELAY_STATE_MS, after_btn="a")
        add("theme step (A)", DELAY_STATE_MS, after_btn="a")
        add("play First by Tig (B)", DELAY_PLAY_MS, after_btn="b")
        print(f"play wait {args.play_wait}s…")
        time.sleep(args.play_wait)
        add(f"still playing ~{args.play_wait:.0f}s", DELAY_PLAY_MS)
        add("pause via A (no theme change)", DELAY_STATE_MS, after_btn="a")

        gif = out / "xuss-c-demo.gif"
        frames[0].save(
            gif,
            save_all=True,
            append_images=frames[1:],
            duration=delays,
            loop=0,
            optimize=False,
        )
        # Also pure-product stills index without captions for docs
        print(f"OK wrote {gif} ({len(frames)} frames, delays_ms={delays})")
        print(
            "capture_note: host-paced max ~0.054 fps @ 115200; "
            "GIF delays make playback smooth, not capture rate"
        )
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
