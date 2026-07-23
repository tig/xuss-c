# Xuss-C Software Specification

**Rev 0.3, July 2026**

Xuss-C is a pocket companion for the M5Stack **M5GO IoT Starter Kit v2.7**. It boots with a short musical greeting, shows a living face on the screen, plays a full song on demand, and exposes a simple details screen for the sensors built into the M5GO core.

This is the **C-language twin** of [tig/xuss](https://github.com/tig/xuss). Product shape matches that contract: same hardware, same face, same song, same buttons, same Details screen. The intentional differences are the ones that matter for usability:

- **Runtime:** native **C on ESP-IDF** (not MicroPython on [silico](https://github.com/tig/silico)).
- **Audio:** clean, continuous sample playback without the MicroPython path’s dropouts and low-fidelity compromises. We should consider utilizing https://github.com/Shreyas-M/ESP32-Synth.
- **Multitasking:** the living face, buttons, Details, and USB serial stay responsive **while music plays** — no frozen UI, no “pause first to open Details,” no multi-minute deafness to the link.

This document is the product contract. The first half is written for the person who owns the device. The second half is written for the implementer who must rebuild that experience on ESP-IDF without guessing at product intent.

Host/board techniques (ESP32 DAC lifecycle, DMA audio, IPS color packing, large-asset flash, M5GO power) belong in project or silico knowledge notes — not as product moat here.

---

## Working Backwards Artifact: Xuss-C User's Manual

### Welcome

Thank you for powering on Xuss-C.

Xuss-C is a small desk toy that lives on your M5GO. It smiles, winks, changes colors, plays music, and can show a live look at its built-in sensors. It is intentionally simple: three front buttons do almost everything.

Xuss-C is the same companion as Xuss, rebuilt in native C so music stays clear and the face keeps living while the song plays.

### What's in the box (what you need)

- An **M5Stack M5GO IoT Starter Kit v2.7** (the core unit with screen, speaker, three front buttons, and side LED strips)
- USB power (the cable that came with your M5GO)
- Xuss-C software already loaded on the device

No extra modules, knobs, or sensors are required for the features in this manual. Xuss-C uses only what is built into the M5GO core.

### First power-on

1. Plug the M5GO into USB power.
2. Within about two seconds you should hear a short musical riff. That riff is a slice of the song **First** by Tig (a few seconds from the middle of the track).
3. After the riff, Xuss-C shows its face and waits.

If you hear nothing and see nothing, check the USB cable and power. If the face appears but sound is missing, try another cable or port; the speaker needs a healthy USB supply.

### The face (home screen)

When Xuss-C is idle, the screen shows:

- A **smiley face** (eyes and a simple smile)
- A **scrolling banner** along the top of the screen (the "hair"), reading:

  > Xuss-C; built on ESP-IDF

- Soft **button hints** along the bottom of the screen, above the physical buttons:
  - Left: **color**
  - Middle: **play** / **pause** symbol
  - Right: **gear** symbol

The side LED strips light in the same color family as the face.

Every **ten seconds**, the right eye gives a short wink.

**While music is playing**, the face **keeps living**: the banner still scrolls, the wink still happens, and the middle-button hint shows pause. You do not lose the companion face to a frozen “now playing only” screen.

### Colors (left button)

Press the **left button** (Button A) to cycle the look of the face and the side lights **when you are on the face (not playing music)**:

| Step | Name | What you see |
|---|---|---|
| 1 | Blue (default) | Bright blue face on a dark blue background; blue side lights |
| 2 | Orange | Warm orange face and sides on a dark orange-tinted background |
| 3 | Red | Red face and sides on a dark red background |
| 4 | Green | Green face and sides on a dark green background |
| 5 | Black | Black face on a **white** background; side lights **off** |

Press again after Black to return to Blue. The banner (hair) follows the color theme.

Hold does not fast-forward: one press, one step.

**While a song is playing**, the left button does **not** change colors. It **pauses** the music and leaves you on the face with the same theme as before.

### Music (middle button)

Xuss-C can play the full track **First** by Tig through the M5GO speaker.

| Action | What happens |
|---|---|
| Press **middle** (Button B) while idle | Playback starts from the beginning. The face **stays** on screen and keeps animating; the middle-button hint becomes a pause symbol. A small **playing** cue (title **First by Tig**) is visible so you know music is on. |
| Press **middle** while the song is playing | Playback **pauses** and remembers where you left off. The middle-button hint becomes a play symbol. |
| Press **middle** while paused | Playback **resumes** from the pause point (it does not restart). |
| Let the song finish | Playback stops at the end. It does **not** loop or auto-restart. Press middle again to play from the start. |

Notes:

- Playback is **sample audio** (not a square-wave beeper tone for music). Native C drives the speaker path with continuous buffered PCM so the track does not stutter while the face moves or the serial link answers.
- Sound is still limited by the tiny M5GO speaker, but the digital path is built for clean desk listening: no intentional low-fidelity downsampling beyond what the speaker path requires, and no UI freezes to “make room” for audio.
- You can open **Details** (right button) **while music is playing**; music keeps going in the background.
- If you press **left** while music is playing, Xuss-C **pauses**, stays on the **face**, and does **not** change the color theme.

There is no volume knob on the front panel in this version. Volume is fixed at a comfortable desk level unless a technician changes it over the serial link (see the technical section).

### Details / sensors (right button)

Press the **right button** (Button C) to open the **Details** screen.

You will see:

- **Firmware name and version** at the top (for support and updates) — digits must be readable, not solid blocks
- **Built-in motion sensor** (accelerometer, gyroscope, and a temperature reading from that chip)
- **Front button states** (which of A / B / C are currently held)
- Other built-in board readings when available (for example free memory)

Numbers update about **ten times per second** so you can tilt the device and watch the motion values change. Labels stay put; only the numbers refresh.

| Control on Details | Effect |
|---|---|
| **Right button** (gear) again | Does nothing (you are already on Details) |
| **Left button** (color) | Leaves Details and returns to the face **without** changing the color theme (music keeps its play/pause state) |
| **Middle button** (play/pause) | Same music rules as on the face |

Opening Details does **not** force a pause. If the song was playing, it keeps playing while you read sensors.

Details uses only sensors **inside** the M5GO core. Plug-in modules on the Grove ports are not part of this screen.

### Power on and off (battery)

Xuss-C uses the M5GO’s own power control. There is no separate Xuss-C power key.

While running **on battery** (USB unplugged):

- **Single-click** the red power button on the side to **turn the device on**.
- **Quick double-click** the red power button to **turn the device off**.

Long-press is not how this hardware powers down. If the unit is still warm after a double-click, try again with two short taps.

While **USB is plugged in**, the M5GO stays powered from the cable (and typically charges the battery). Unplug USB first if you want a true battery-style off.

Some M5GO / Core v2.7 units also have a small **hardware battery switch** on the bottom or back that fully isolates the pack (`0` = battery disconnected). Use that as a hard kill if you need the battery cut completely.

### Everyday use tips

- **Desk use**: leave it plugged into USB for all-day play, or run from battery and double-click the red button when you are done.
- **Music and face together**: play the song and watch the face keep winking and scrolling. That is the product working as designed.
- **Music and Details together**: open the gear screen mid-song to watch the IMU while the track continues.
- **Support identity**: the Details screen shows the firmware version. A technician can also read `fw_name` / `fw_version` over the USB serial link even while music is playing.

### Safety and care

- Indoor desk use only.
- Do not cover the speaker or press hard on the screen.
- Keep liquids away from the open ports.
- Use a quality USB cable and a normal computer or USB charger.

### What this version is not

This version of Xuss-C is a **friendly face and music demo** on the M5GO. It is not a phone, not a full media player with a playlist browser, and not a kit that requires soldering or extra modules. It is not an engine-speed or bench-instrument product.

---

## 1. Product intent (for implementers)

Rebuild the experience in the User's Manual on an M5GO v2.7 in **C on ESP-IDF**, driven from this spec:

- Boot musical greeting from *First* (sample PCM)
- Idle face, time-based wink, scrolling hair banner
- Five color themes (including black/white)
- Full *First* play / pause / resume / no loop
- **Living face during full-song audio** (wink + banner continue; play state visible)
- Details screen: firmware + built-in core sensors only; **usable while music plays**
- Three front buttons with on-screen hints
- USB serial: identity + escape hatch + small commissioning params
- **Speaker path: sample PCM only** for this rev (no LEDC PWM on the speaker pin)

**Twin relationship:** product outcomes match [tig/xuss](https://github.com/tig/xuss) Rev 0.3 unless this document **explicitly diverges**. Intentional divergences:

| Topic | Xuss (MicroPython) | Xuss-C (this spec) |
|---|---|---|
| Runtime | MicroPython on silico | C on ESP-IDF |
| Identity | `fw_name=XUSS` | `fw_name=XUSSC` |
| Banner | `Xuss; built with Silico` | `Xuss-C; built on ESP-IDF` |
| UI while music plays | May switch to frozen Now Playing | Living face + playing cue |
| Details while music plays | Pause first, then Details | Details without pausing music |
| Audio quality bar | Low-fidelity path accepted | Continuous buffered PCM; no UI/audio monopoly; higher sample rate preferred |
| Multitasking model | Cooperative single loop with yield rules | Concurrent concerns (RTOS tasks or equivalent); audio never freezes UI or link |
| Escape hatch `repl` | MicroPython prompt | Park outputs; return a flashable USB-console state (no MicroPython) |

**Not product requirements for Rev 0.3** (do not reintroduce as the product):

- ANGLE-knob live RPM, PIR greet, edge/tach profiles (`sing` / `run`), Grove modules on Details, multi-track library, bench-instrument dead-man rails

## 2. Readiness layers (definition of done)

| Layer | Proven when |
|---|---|
| **L0 (host)** | Face timing, theme cycle, song state machine, Details layout, and protocol parsing pass automated tests against a HAL double or pure logic (CTest or equivalent; no hardware required). |
| **L1 (metal)** | On a real M5GO: boot riff audible and clean, face and banner visible **during and after** the riff settles, buttons match the manual, full song play/pause/resume works **without audible underruns while the face animates**, Details shows live built-in sensors **while a song is playing**, escape hatch works **including while a song is playing**, identity on the link mid-track. |

Claims name their layer. Host green is never metal done. Self-report is not measurement for audio or display quality: an operator must see and hear the product face.

## 3. Hardware (fixed)

M5Stack **M5GO IoT Starter Kit v2.7**. No soldering. No required external units.

| Piece | Role |
|---|---|
| M5GO core (ESP32, flash, 2.0" 320×240 IPS, speaker, three front buttons, 6-axis IMU) | Face, audio, buttons, Details sensors |
| Ten side RGB LEDs | Theme-matched side light |
| Side red power button (+ battery base) | On/off on battery: single-click on, quick double-click off (M5GO / Core v2.7). USB power keeps the unit on. |

Use only **built-in** sensors on the Details screen (IMU on the internal bus, front buttons, and other core-only readings such as heap free). Do not depend on Port A/B/C modules for Rev 0.3 product behavior.

Power is **hardware-owned** by the M5GO power path (not an Xuss-C software feature). Product firmware must not fight the power button or require a long-press convention that this board does not implement.

**Speaker:** product audio is **unsigned 8-bit mono PCM** on the board speaker DAC path (ESP32 built-in DAC is 8-bit). Do **not** drive LEDC PWM on the speaker pin in this revision (avoids destroying DAC sample quality for the rest of the session). Prefer a **sample rate of 22,050 Hz** (or higher if flash budget allows and the path remains clean) for full-track assets; boot riff may share the same rate. Continuous DMA (or equivalent) buffering is expected so UI work does not cause underruns.

First firmware flash is esptool (ESP32). Subsequent app updates use the project flash path (`idf.py` / esptool). The ASCII protocol escape hatch remains required so agents can reclaim the console without a hard reset when possible.

## 4. User-visible behavior (normative)

### 4.1 Boot

1. Emit the identity line on the USB serial link first (`fw_name=XUSSC fw_version=…`).
2. Play the boot riff: a short excerpt of *First* (the asset is seconds ~7.5–10 of the track; unsigned 8-bit mono PCM at the project sample rate). Playback may complete before the idle face loop is fully lively, but identity must not wait on the riff. Prefer starting the face chrome as soon as practical **while** the riff finishes if the multitasking path allows it without audio glitches.
3. Enter the idle face with the default **blue** theme.

Boot riff end must not click or hard-cut into silence; ease out cleanly.

### 4.2 Idle face

- Eyes, smile, and theme colors on the IPS panel.
- Hair banner scrolls smoothly right → left with text **`Xuss-C; built on ESP-IDF`** (note the semicolon). Banner motion updates only the hair-bar region (not a full-panel redraw every step).
- Right-eye wink on a **time-based** ~10 s period (not "every N control ticks"). A wink **repaints only the affected eye** (open ↔ closed). It must not clear or redraw the whole screen, the other eye, the smile, the banner, or the button hints.
- Side LEDs match the active theme; **black** theme forces side LEDs fully off and uses a white background with black face features.
- Bottom of the panel shows persistent hints: **color** / play-or-pause glyph / gear glyph.

Face animation timing is wall-clock based, never "tick count" based. Prefer regional updates (eye, banner strip, value fields) over full-frame fills whenever only part of the picture changed.

**While full-song audio is active**, the same face remains the primary view: wink and banner **continue**. Show a clear playing cue that includes the title **First by Tig** (for example a compact status line or title under the face). Do **not** replace the living face with a static full-screen freeze.

**Glyph set:** every on-screen string the product shows (banner, labels, firmware version, sensor values, song title) must render with a font that includes at least space, `0–9`, `+`, `-`, `.`, and the letters used in product copy. Missing glyphs that draw as solid blocks are a product defect.

### 4.3 Themes (Button A)

Cycle order is fixed:

`blue → orange → red → green → black → blue → …`

One edge per press (debounce). Themes retint face, hair bar, banner ink, and side LEDs together.

| Context | Button A |
|---|---|
| Face (not playing) | Next theme |
| Full-song **playing** | **Pause**, remain on **face**, **do not** advance theme |
| Details | Exit to face, **do not** advance theme (music state unchanged) |

### 4.4 Music (Button B)

Full-track asset: entire *First* as unsigned 8-bit mono PCM at the project sample rate, **streamed from on-device storage** (not held entirely in RAM). Deploy must place the file on the device filesystem (or embedded partition) and **verify non-zero size**. If the file is missing or empty, refuse playback with a clear link status (and do not hang the UI).

State machine:

| State | Button B | Result |
|---|---|---|
| idle | press | start from byte 0; show playing cue on face |
| playing | press | pause; keep resume offset; clear playing cue |
| paused | press | resume from offset; show playing cue |
| natural end | — | idle, offset 0, no auto-repeat |

When not actively playing (paused, finished, or error), restore non-playing chrome on the current screen (face or Details).

`mute=1` (if set) blocks starting playback and reports a clear status; it does not crash the UI.

**Audio quality (normative for this twin):**

1. Sample PCM only for *First* and the boot riff (no PWM square wave on the speaker for music).
2. Continuous buffered playback: no intentional stop-the-world hold of the UI loop for the whole track.
3. No audible underruns or periodic stutter correlated with wink, banner scroll, Details refresh, or serial service under normal desk load.
4. Clean start and stop (no click into silence at pause or natural end when avoidable with a short fade or DC settle).

### 4.5 Details (Button C)

- From face (idle, paused, **or playing**): open Details **immediately**. Do **not** force a pause.
- While Details is already visible: Button C is a **no-op**.
- Details shows firmware identity at the top and live built-in sensor values beneath.
- Sensor values refresh every **100 ms** (~10 Hz) as a **visual** rate. Labels and chrome are stable; only value fields update (partial screen update, not a full-panel flash every sample).
- Required readings when hardware is present: IMU acceleration, rotation rate, IMU temperature; front button levels; optional core extras (e.g. free memory) if cheap to obtain.
- Music state is independent: play/pause/resume from Details follows §4.4; leaving Details via Button A does not change play/pause.

### 4.6 Button map (summary)

| Button | Face (idle/paused) | Playing (face + cue) | Details |
|---|---|---|---|
| A (left) | Next theme | Pause → face, **no** theme change | Exit to face, no theme change |
| B (middle) | Play / resume | Pause | Play / pause / resume (same rules) |
| C (right) | Open Details | Open Details (**music continues**) | No-op |

## 5. Multitasking model (normative)

Xuss-C runs on ESP-IDF and **must** treat concurrent concerns as first-class. A single-threaded “pump audio until done, UI waits” design fails this section even if buttons eventually work after the song ends.

### 5.1 Concurrent concerns

The product simultaneously owns these concerns:

1. **UI / face** — theme, wink, banner motion, playing cue, Details chrome
2. **Input** — debounced front buttons
3. **Audio** — boot riff and full-song PCM to the speaker
4. **Sensors** — Details sampling when that screen is active
5. **Link** — USB serial identity, commissioning params, and the escape hatch

### 5.2 Rules

1. **Concurrent ownership.** Use FreeRTOS tasks (or an equivalent structured concurrency model on ESP-IDF) so audio production is not starved by UI and UI is not starved by audio. A cooperative main loop is acceptable only if it is **proven** to meet the acceptance sketch below with continuous PCM and living face motion; “eventually services buttons after the track” is not enough.
2. **No monopoly on long audio.** Full-song PCM must **never** block:
   - Button A (pause → face, no theme change), Button B (pause), and Button C (open Details **without** pause);
   - face wink and banner motion on wall-clock schedule;
   - Details value refresh when Details is visible;
   - **USB serial** so `identity`, `repl`, and `reboot` work **while the song is playing**. Bounded intake/egress per service turn is fine; multi-minute deafness to the link is not.
3. **UI honesty during music.** While full-song audio is active, the living face (or Details) remains usable. Freezing wink/banner for the whole track is a **defect** on Xuss-C (it is tolerated on MicroPython Xuss; it is not tolerated here).
4. **Boot riff.** The short boot greeting may start before every steady-state task is fully spun up, provided identity is emitted first and the riff ends cleanly. Prefer overlapping face bring-up with the riff when safe for audio quality.
5. **Details sampling.** Refresh sensor values every **100 ms** (visual), including while music plays. Only value fields update; chrome stays put.
6. **Face motion is time-based.** Banner position and wink schedule derive from elapsed time, not from "how many loop iterations ran."
7. **Regional paints.** Wink → eye only. Banner scroll → hair bar only. Details numbers → value strips only. Full-screen clears are for mode changes (theme, enter/leave Details), not for routine animation or routine audio buffers.

### 5.3 Acceptance sketch for multitasking

| Check | Pass when |
|---|---|
| Pause while playing | Middle button pauses within a short, human-noticeable delay mid-track |
| A while playing | Left button pauses, shows face, theme index unchanged |
| Details while playing | Right button opens Details **without** stopping music; song continues audibly |
| Living face while playing | Wink and/or banner motion visible mid-track without pausing |
| Link while playing | `identity` and `repl` succeed mid-track without requiring a prior pause |
| No audio/UI fight | Operator hears continuous music while face animates; no regular stutter on each wink/banner step |
| Idle life | With no song playing, wink and banner continue indefinitely |

## 6. Manners and rails

1. **Identity first.** Boot prints `fw_name` / `fw_version` before other chatter.
2. **Escape hatch from day one.** `repl` parks outputs (speaker, LEDs as needed) and returns a **usable flashable USB-console state** so a host can redeploy without hardware gymnastics; `reboot` parks and hard-resets. There is no MicroPython prompt on this twin. A build without the door fails L1. Must work mid-song (§5.2).
3. **Serial is bounded.** Byte-budgeted intake and egress per service turn; malformed input fails closed with a short error; Ctrl-C on the link is data, not a forced interrupt, while the product owns the console.
4. **Self-report is not measurement.** "I think the DAC worked" is not L1. An operator hears the riff and sees the face — including face motion during music.
5. **Hardware honesty.** Prefer measure-then-fix on this board (display color packing, speaker path, IMU address, DMA buffer sizing) over folklore. Promote reusable truths to silico knowledge or this repo’s notes when they help the next agent.

## 7. Protocol and parameters (canonical allow-list)

ASCII line protocol on USB serial. **This is the complete product command surface for Rev 0.3.** Do not reintroduce instrument verbs as product requirements.

### 7.1 Required commands

| Command | Behavior |
|---|---|
| `identity` | Returns `fw_name=… fw_version=…` |
| `repl` | Escape hatch: park outputs, release product ownership of the console into a flashable/idle host-serviceable state |
| `reboot` | Park outputs, hard reset |

### 7.2 Optional commissioning

| Command / param | Behavior |
|---|---|
| `get` / `set` / `save` / `defaults` | Only if you persist commissioning state |
| `mute` | `0`/`1`; when `1`, blocks starting *First*; may survive `defaults` if you keep an exempt list |
| `volume` | Integer desk level if exposed; default is a comfortable fixed level |
| `telemetry_hz` | Optional; `0` = off |

### 7.3 Not product requirements

Do **not** require for Rev 0.3: `rpm`, `route`, `sing`, `run`, `stop` (as instrument), `knob`, `greet`, `ring_teeth`, named engine profiles, or ANGLE/PIR units.

Config persistence, if present, must fall back safely when the on-device image is torn or alien.

## 8. Acceptance (camera / operator)

| Row | Check | Layer |
|---|---|---|
| Boot riff | Greeting within ~2 s of power; identity line first; riff recognizable as *First*; clean ending | L1 |
| Face | Idle face, scrolling banner text correct (`Xuss-C; built on ESP-IDF`), ~10 s wink visible | L1 |
| Themes | A cycles blue → orange → red → green → black → blue; sides match; black sides off / white bg | L1 |
| Play / pause / resume | B starts full track; B pauses mid-song; B resumes (not restart); end does not loop | L1 |
| Living face while playing | Mid-track, wink and/or banner still move; title **First by Tig** is visible as playing cue | L1 |
| Clean audio mid-UI | Full track plays without regular stutter while face animates | L1 |
| A while playing | A pauses and returns to / stays on face; theme index unchanged | L1 |
| Details | C opens sensor screen with firmware line (readable digits); values move when tilted; ~100 ms updates without full-screen flash | L1 |
| Details while playing | C opens Details **without** pausing; music continues | L1 |
| Wink paint | Idle wink changes only the right eye; rest of the face does not flash | L1 |
| C on Details | Second C does nothing | L1 |
| A on Details | Returns to face without theme change | L1 |
| Link mid-song | `identity` works while track is playing; `repl` exits clean mid-song | L1 |
| Missing song file | Clear failure if full-track asset absent; UI remains usable | L1 |
| Escape hatch | After `repl`, redeploy possible without hardware gymnastics | L1 |
| PCM speaker path | Music is sample audio; product does not use PWM square wave on the speaker for *First* | L1 |

## 9. The build (for the agent that gets pointed here)

ESP-IDF is the build and runtime: https://docs.espressif.com/projects/esp-idf/. Work spec-first: this document is the contract; do not edit it, and carry an ambiguity log in the PR for every place it made you guess. Twin reference: [tig/xuss](https://github.com/tig/xuss) for product behavior; do not copy MicroPython modules blindly, and do not reintroduce the old bench-instrument surface. The gates are host unit tests (`cmake` + `ctest` for pure logic / HAL doubles) and a successful metal flash; both are part of done once code exists.
