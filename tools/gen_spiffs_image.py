"""Build a SPIFFS image from assets/ for idf flash (first.pcm + optional boot)."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--idf-path", required=True)
    ap.add_argument("--assets", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--size", default="0xE70000")
    ap.add_argument("--page-size", type=int, default=256)
    ap.add_argument("--block-size", type=int, default=4096)
    args = ap.parse_args()

    spiffsgen = Path(args.idf_path) / "components" / "spiffs" / "spiffsgen.py"
    if not spiffsgen.is_file():
        print(f"spiffsgen not found: {spiffsgen}", file=sys.stderr)
        return 2

    staging = args.out.parent / "spiffs_image"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    first = args.assets / "first.pcm"
    if not first.is_file() or first.stat().st_size <= 0:
        print(f"missing non-empty {first}", file=sys.stderr)
        return 3
    shutil.copy2(first, staging / "first.pcm")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable,
        str(spiffsgen),
        args.size,
        str(staging),
        str(args.out),
        "--page-size",
        str(args.page_size),
        "--block-size",
        str(args.block_size),
    ]
    print(" ".join(cmd))
    subprocess.check_call(cmd)
    print(f"wrote {args.out} ({args.out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
