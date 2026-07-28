#!/usr/bin/env python3
"""Thin wrapper: prefer installed esprec CLI for Xuss-C screenshots.

Kept so older docs that say ``python tools/shot.py`` still work.
"""

from __future__ import annotations

import sys


def main() -> int:
    try:
        from esprec.cli import main as esprec_main
    except ImportError:
        print(
            "esprec not installed. From tig/esprec: pip install -e .[dev]\n"
            "Then: esprec snapshot --port COMx -o face.png",
            file=sys.stderr,
        )
        return 2
    # Map legacy flags: tools/shot.py --port X -o Y → esprec snapshot
    argv = ["snapshot"]
    args = sys.argv[1:]
    i = 0
    frames = 1
    hz = 2.0
    while i < len(args):
        a = args[i]
        if a in ("--frames",) and i + 1 < len(args):
            frames = int(args[i + 1])
            i += 2
            continue
        if a in ("--hz",) and i + 1 < len(args):
            hz = float(args[i + 1])
            i += 2
            continue
        if a in ("-o", "--output") and i + 1 < len(args):
            argv.extend(["-o", args[i + 1]])
            i += 2
            continue
        argv.append(a)
        i += 1
    if frames > 1:
        argv[0] = "record"
        argv.extend(["--frames", str(frames), "--hz", str(hz)])
    # Prefer legacy "shot" command (device accepts shot and esprec shot).
    if "--command" not in argv:
        argv.extend(["--command", "shot"])
    return esprec_main(argv)


if __name__ == "__main__":
    raise SystemExit(main())
