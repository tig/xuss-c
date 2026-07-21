# AGENTS.md

Guidance for AI coding agents working in **xuss-c**.

## What this repo is

A GCU (one shippable edge device) with the **same product contract as [tig/xuss](https://github.com/tig/xuss)**, implemented in **C on ESP-IDF** instead of MicroPython on silico's plate.

Silico remains the Day 1 manners / host-path reference for the twin product. This repo does **not** invent a different instrument; it changes the runtime language and audio pipeline.

## How to work here

1. **Read `spec.md` first and take it literally.** It is the contract. Do not edit it; if you believe it is wrong, say so in your PR instead.
2. Work on a branch, PR to `main`, do not merge. The PR body carries an **ambiguity log**.
3. Host gates: `cmake` + `ctest` under `host/` (edge math, config, protocol). All green is part of L0 done.
4. Ground in part truth: `parts.toml` pointers. Never commit fetched documents.
5. Hardware honesty: claim readiness per spec §1. Self-report is not measurement.
6. **Before implementing the voice path**, investigate [ESP32-Synth](https://github.com/The-Shreyas-M/ESP32-Synth) as required by the spec (I2S + internal DAC, dithering, FreeRTOS audio task). Record findings in the PR ambiguity log: adopt, adapt, or reject with reason.
7. First flash is esptool (ESP32). Escape hatch (`repl`/`reboot` over the ASCII link) is required from day one; a build without the door fails L1.
8. Do not soft-fork bedside/silico manners into a parallel essay; keep this file short.
