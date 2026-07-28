#!/usr/bin/env python3
"""Pull a shadow-framebuffer screenshot from Xuss-C over USB serial.

Protocol (device) — base64 so cooked console CRLF cannot corrupt 0x0A pixels:
  Host writes:  shot\\n
  Device prints: SHOT w=320 h=240 fmt=rgb565be enc=b64 nbytes=N crc=0x...
  Device prints: base64 lines (76 cols)
  Device prints: SHOT_END crc=0x...

Example:
  python tools/shot.py --port COM7 -o capture/face.png
  python tools/shot.py --port COM7 --frames 8 --hz 2 -o capture/clip
"""

from __future__ import annotations

import argparse
import base64
import binascii
import re
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError as e:
    print("pyserial required: pip install pyserial", file=sys.stderr)
    raise SystemExit(2) from e

try:
    from PIL import Image
except ImportError as e:
    print("Pillow required: pip install Pillow", file=sys.stderr)
    raise SystemExit(2) from e

HEADER_RE = re.compile(
    r"^SHOT\s+w=(?P<w>\d+)\s+h=(?P<h>\d+)\s+fmt=(?P<fmt>\S+)\s+"
    r"(?:enc=(?P<enc>\S+)\s+)?"
    r"nbytes=(?P<nbytes>\d+)\s+crc=(?P<crc>0x[0-9A-Fa-f]+)\s*$"
)


def rgb565_shadow_to_rgb(buf: bytes, w: int, h: int) -> Image.Image:
    """Convert device shadow (LE memory of SPI byte-swapped RGB565) to RGB."""
    need = w * h * 2
    if len(buf) < need:
        raise ValueError(f"short frame: got {len(buf)} want {need}")
    out = bytearray(w * h * 3)
    o = 0
    for i in range(0, need, 2):
        # uint16 as stored on LE ESP32 (same words sent to the panel DMA).
        wire = buf[i] | (buf[i + 1] << 8)
        # Undo firmware rgb565() byte-swap → logical 565 (R in high bits).
        pix = ((wire & 0xFF) << 8) | ((wire >> 8) & 0xFF)
        r = (pix >> 11) & 0x1F
        g = (pix >> 5) & 0x3F
        b = pix & 0x1F
        out[o] = (r << 3) | (r >> 2)
        out[o + 1] = (g << 2) | (g >> 4)
        out[o + 2] = (b << 3) | (b >> 2)
        o += 3
    return Image.frombytes("RGB", (w, h), bytes(out))


def grab_one(ser: serial.Serial, timeout_s: float = 60.0) -> tuple[Image.Image, dict]:
    ser.reset_input_buffer()
    ser.write(b"shot\n")
    ser.flush()
    deadline = time.monotonic() + timeout_s
    header = None
    while time.monotonic() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("utf-8", errors="replace").strip()
        if text.startswith("SHOT_ERR"):
            raise RuntimeError(text)
        m = HEADER_RE.match(text)
        if m:
            header = m.groupdict()
            break
    if not header:
        raise TimeoutError("no SHOT header from device")

    w = int(header["w"])
    h = int(header["h"])
    nbytes = int(header["nbytes"])
    crc_hdr = int(header["crc"], 16)
    fmt = header["fmt"]
    enc = (header.get("enc") or "raw").lower()
    if fmt != "rgb565be":
        raise RuntimeError(f"unsupported fmt {fmt}")

    b64_parts: list[str] = []
    raw = bytearray()

    if enc in ("b64", "base64"):
        while time.monotonic() < deadline:
            line = ser.readline()
            if not line:
                continue
            text = line.decode("utf-8", errors="replace").strip()
            if text.startswith("SHOT_END"):
                break
            if text.startswith("SHOT"):
                continue
            # base64 alphabet only
            if re.fullmatch(r"[A-Za-z0-9+/=]+", text):
                b64_parts.append(text)
        try:
            s = "".join(b64_parts)
            s += "=" * ((4 - (len(s) % 4)) % 4)
            raw = bytearray(base64.b64decode(s, validate=False))
        except Exception as e:
            raise RuntimeError(
                f"base64 decode failed: {e} (parts={len(b64_parts)} chars={sum(len(p) for p in b64_parts)})"
            ) from e
    else:
        # Legacy raw path (fragile on cooked consoles)
        ser.timeout = 2.0
        while len(raw) < nbytes and time.monotonic() < deadline:
            chunk = ser.read(min(4096, nbytes - len(raw)))
            if chunk:
                raw.extend(chunk)
        while time.monotonic() < deadline:
            line = ser.readline()
            if not line:
                continue
            if line.decode("utf-8", errors="replace").strip().startswith("SHOT_END"):
                break

    if len(raw) < nbytes:
        raise TimeoutError(f"short payload {len(raw)}/{nbytes}")
    raw = raw[:nbytes]

    crc_calc = binascii.crc32(raw) & 0xFFFFFFFF
    meta = {
        "w": w,
        "h": h,
        "nbytes": nbytes,
        "crc_header": crc_hdr,
        "crc_calc": crc_calc,
        "fmt": fmt,
        "enc": enc,
    }
    img = rgb565_shadow_to_rgb(bytes(raw), w, h)
    return img, meta


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True, help="COMx or /dev/tty…")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("-o", "--output", default="capture/face.png")
    ap.add_argument("--frames", type=int, default=1, help=">1 for a short clip")
    ap.add_argument("--hz", type=float, default=2.0, help="frame rate when frames>1")
    ap.add_argument("--settle", type=float, default=1.5, help="seconds after open before shot")
    args = ap.parse_args()

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)

    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    try:
        ser.dtr = False
        ser.rts = False
        time.sleep(args.settle)
        ser.reset_input_buffer()

        if args.frames <= 1:
            img, meta = grab_one(ser)
            img.save(out)
            print(
                f"OK wrote {out} {meta['w']}x{meta['h']} "
                f"enc={meta['enc']} crc=0x{meta['crc_header']:08x}"
            )
            if meta["crc_calc"] != meta["crc_header"]:
                print(
                    f"WARN crc mismatch calc=0x{meta['crc_calc']:08x}",
                    file=sys.stderr,
                )
            return 0

        period = 1.0 / args.hz if args.hz > 0 else 0.5
        frames: list[Image.Image] = []
        stem = out.with_suffix("") if out.suffix else out
        for i in range(args.frames):
            t0 = time.monotonic()
            img, meta = grab_one(ser)
            path = Path(f"{stem}_{i:03d}.png")
            path.parent.mkdir(parents=True, exist_ok=True)
            img.save(path)
            frames.append(img)
            print(f"OK frame {i} {path} {meta['w']}x{meta['h']}")
            elapsed = time.monotonic() - t0
            time.sleep(max(0.0, period - elapsed))
        gif_path = Path(f"{stem}.gif")
        frames[0].save(
            gif_path,
            save_all=True,
            append_images=frames[1:],
            duration=int(period * 1000),
            loop=0,
        )
        print(f"OK wrote clip {gif_path} ({len(frames)} frames @ {args.hz} Hz)")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
