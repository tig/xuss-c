# AGENTS.md

Guidance for AI coding agents working in **xuss-c**.

## What this repo is

A GCU (one shippable edge device) with the **same product contract as [tig/xuss](https://github.com/tig/xuss)**, implemented in **C on ESP-IDF** instead of MicroPython on silico's plate.

Silico remains a useful Day 1 manners / host-path reference for the twin product. This repo does **not** invent a different instrument; it changes the runtime language and build path.

## How to work here

1. **Read `spec.md` first and take it literally.** It is the contract. Do not edit it; if you believe it is wrong, say so in your PR instead.
2. Work on a branch, PR to `main`, do not merge. The PR body carries an **ambiguity log**: every place the spec was silent or ambiguous, and the choice you made.
3. The gates are host unit tests (`cmake` + `ctest` for pure logic) and a successful ESP-IDF flash of the metal app. Both are part of done once code exists.
4. Ground in part truth before writing hardware-facing code: follow the pointers in `parts.toml`. Never commit fetched documents.
5. Hardware honesty: claim readiness per spec §1. Self-report is not measurement (§6 rail 4).
6. First flash is esptool (ESP32). Subsequent app updates use the project flash path (`idf.py` / esptool). The escape hatch (`repl`/`reboot` over the ASCII link) is required from day one; a build without the door fails L1.
