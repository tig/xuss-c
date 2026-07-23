# Xuss-C

Xuss-C is a pocket companion for the M5Stack **M5GO IoT Starter Kit v2.7**: a living face, a short boot riff, full-song playback of *First*, and a built-in sensor Details screen.

Same product shape as [tig/xuss](https://github.com/tig/xuss), intended as a **native C / ESP-IDF** twin so audio stays clean and the UI stays lively while music plays.

The name is the short half of Turminder Xuss, the drone in Iain M. Banks' *Matter*: precise dirty work while the human keeps judgment.

This repository is a product example used with [Silico](https://github.com/tig/silico).

## Status

**Spec only.** [spec.md](spec.md) is the product contract (Rev 0.3). No firmware or host layout is checked in yet.

## Hardware

- **M5Stack M5GO IoT Starter Kit v2.7** (core with screen, speaker, three front buttons, side LED strips)
- USB power / data cable

No extra modules are required for the features in the spec.

## Clean start (maintainers)

Tag **`clean-start`** is the product-docs-only baseline. Reset `main` to it:

```text
git fetch origin tag clean-start
git switch main
git reset --hard clean-start
git push --force origin main
```

See `git show clean-start` for the annotated recipe. Keep product truth here; keep Silico host and agent guidance in the silico repo, not in this tree.
