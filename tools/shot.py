#!/usr/bin/env python3
"""Thin wrapper: Xuss-C screenshots via esprec public API / CLI.

Prefer::

  esprec snapshot --port COMx -o face.png
  python tools/screen_scenario.py --port COMx -o capture/
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
    argv = ["snapshot"]
    args = sys.argv[1:]
    i = 0
    frames = 1
    hz = 2.0
    while i < len(args):
        a = args[i]
        if a == "--frames" and i + 1 < len(args):
            frames = int(args[i + 1])
            i += 2
            continue
        if a == "--hz" and i + 1 < len(args):
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
    if "--command" not in argv:
        argv.extend(["--command", "shot"])
    return esprec_main(argv)


if __name__ == "__main__":
    raise SystemExit(main())
