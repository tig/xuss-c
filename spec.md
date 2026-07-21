# Xuss-C Software Specification (draft)

**Rev 0.1, July 2026**

Xuss-C is a bench drone with two jobs. The day job: a test instrument that generates engine-speed edge trains, forces inputs, and measures current for sibling GCUs on the bench. The stage job: proof on camera that a spec plus a C/ESP-IDF build produces a working device. The trick that keeps the demo honest is that the voice *is* the instrument output: an engine-speed edge train is an audible waveform at the commanded edge frequency, and when Xuss-C sings it is demonstrably on-pitch per this spec's own edge-generator section.

This is the **C-language twin** of [tig/xuss](https://github.com/tig/xuss). Product outcomes, hardware, protocol, rails, and acceptance rows match that contract unless this document explicitly diverges. Runtime is **C on ESP-IDF** (not MicroPython).

This is a **requirements** document. It states outcomes, interfaces, and acceptance rows. It does not prescribe every internal API name.

## 1. Readiness layers (definition of done)

| Layer | Proven when |
|---|---|
| **L0 (host)** | Song/choreography compiler, edge math, protocol, and config store pass host unit tests (CTest) against a HAL double or pure logic. |
| **L1 (metal, self)** | Edge output frequency verified by external measurement (logic analyzer); pitch within tolerance on the speaker; escape hatch works; identity on the link. |
| **L2 (fixture)** | Xuss-C, as the bench fixture, runs a sibling GCU's metal gate end to end. Its vehicle is the bench. |

Claims name their layer. Host green is never fixture done.

**Nothing in this spec is TBD-by-measurement.** Xuss-C is the instrument, not the mystery.

## 2. Hardware (fixed)

M5Stack **M5GO IoT Starter Kit v2.7**. No soldering anywhere in the build.

| Piece | Role |
|---|---|
| M5GO core (ESP32, 16 MB flash, 2.0" 320x240 IPS, speaker, mic, three buttons, 6-axis IMU) | The drone: face, voice, buttons |
| Ten side RGB LEDs | The light show |
| Grove Port A (I2C) | Day-job sensors: current-sense unit (INA226 class) |
| Grove Port B (GPIO) | **Tach edge output to the DUT** (Grove pigtail) |
| Grove Port C (UART) | Reserved |
| ANGLE unit | **The throttle**: a human revs the simulated engine by hand |
| PIR unit | Presence: greet once when a human approaches |
| RGB / IR / ENV III / HUB units | Light show extension, spares, ambient garnish |

First firmware flash is esptool (ESP32). Subsequent app updates use the project flash path (idf.py / esptool); the ASCII protocol door remains required so agents can reclaim the console without a hard reset when possible.

## 3. The edge engine (voice and instrument are one section)

One edge engine, routable to two sinks.

- Frequency math: `f_hz = rpm × ring_teeth / 60`. At the defaults, 750 rpm is 1,625 Hz and cranking (200 rpm) is 433 Hz.
- `route` selects the sink: `voice` (speaker), `tach` (Port B), or `both`. On `both`, the two outputs shall agree within 0.1% in **frequency** (and in mark-space when both carry the square edge).
- **Profiles are songs.** A profile is a named, timed list of (rpm, ms) pairs. `sing <profile>` plays it on the voice route; `run <profile>` plays it on the tach route. They are the same code path. Built-ins: `crank_catch_idle`, `redline_sweep`, `stall`.
- `duty_pct` sets the mark-space ratio for the edge (including wrong-sensor impersonation at ~5/95).
- **The ANGLE unit is a live rpm input** when enabled.

### 3.1 Voice path — required investigation (normative)

Before locking the metal voice implementation, the agent **shall investigate**:

**https://github.com/The-Shreyas-M/ESP32-Synth**

ESP32-Synth is an open-source ESP32 digital synthesizer using **I2S + internal DAC**, dual-core FreeRTOS audio, classic waveforms (including square), ADSR, and **software dithering** to reduce 8-bit DAC quantization click.

Investigation outcomes (record in the PR ambiguity log):

1. What of I2S-DAC, FreeRTOS audio task pinning, and dithering is reusable on M5GO G25 speaker without fighting the tach pin (G26) and face bus.
2. Whether Xuss-C voice should be: (a) pure edge square via LEDC/PWM, (b) I2S-DAC square/sine at the edge frequency, or (c) hybrid (boot riff sample on DAC, edge on LEDC).
3. Explicit **adopt / adapt / reject** for each technique, with one-sentence reasons.

Rejecting ESP32-Synth wholesale without reading its Synth/audio loop is a spec violation. Adopting its full keypad/Web-UI surface is **not** required; the product is an instrument, not a polyphonic toy.

Tach to the DUT remains a **hard digital edge** suitable for a sibling GCU tach input. Voice may use a richer DAC path only if frequency agreement on `route both` still holds within 0.1%.

## 4. Face and presence

- The IPS panel is the face: eyes that track state (idle, singing, driving the DUT, fault). The ten side LEDs carry the beat.
- Face patterns are time-based, never tick-based.
- **PIR greet**: when `greet=1` and the PIR sees a human after quiet, Xuss-C greets once (short chirp plus face) and then shuts up.

## 5. Manners

- Boot: identity line first (`fw_name=XUSSC fw_version=…`), then the greeting, then silence until spoken to. **The greeting is seconds 7.5 to 10 of *First*** (`assets/boot-riff.u8.raw`, 11,025 Hz u8 mono, played via DAC/I2S per §3.1 choice). Playback never delays readiness: identity goes out first; the port answers while it sings.
- `mute` is **commissioning state**: it survives `defaults` and clears only by explicit `set mute 0`.
- Every actuation that touches a DUT announces itself on the link before it moves.

## 6. Rails (normative)

1. **Dead-man on everything that touches the DUT.** Host silence beyond the window releases forcing channels to passive.
2. **Serial is bounded both directions.** Byte-budgeted intake per tick; poison-to-newline discard with a capped error response; egress capped per tick; Ctrl-C is data.
3. **The escape hatch ships from day one.** `repl` (park outputs, restore console ownership, exit clean into a flashable state) and `reboot` (park, hard reset). A build without the door fails L1.
4. **Self-report is not measurement.** L1 pitch and frequency rows are verified by external instruments.

## 7. Protocol and parameters

ASCII lines, `key=value` telemetry, same verb family as tig/xuss: `identity`, `get`/`set`/`save`/`defaults`, `repl`, `reboot`, plus `sing`, `run`, `route`, `rpm`, `stop`.

| Parameter | Type | Default | Notes |
|---|---|---|---|
| `ring_teeth` | int (10–400) | 130 | Edge math |
| `rpm` | int (0–8000) | 0 | Live commanded speed; 0 is silence |
| `duty_pct` | int (5–95) | 50 | Mark-space |
| `route` | enum | voice | `voice` / `tach` / `both` |
| `volume` | int (0–10) | 6 | Stage-appropriate |
| `greet` | int (0/1) | 1 | PIR greeting |
| `knob` | int (0/1) | 0 | ANGLE unit drives `rpm` live |
| `mute` | int (0/1) | 0 | **Survives `defaults`** |
| `telemetry_hz` | int (0–100) | 0 | 0 is off (quiet after identity) |

Config persists as a single versioned, checksummed image; torn image falls back to factory; `mute` survives migration when its row parses.

## 8. Acceptance (written for a camera)

| Row | Check | Layer |
|---|---|---|
| Boot riff | Greeting begins within two seconds of power; identity line first; riff recognizably *First* at 7.5s in; port answers while it plays | L1 |
| On-pitch | `rpm 1600`: measured voice frequency = 1600 × 130 / 60 Hz within 1% | L1 |
| Same engine | `route both`: voice and tach frequency identical within 0.1% | L1 |
| Knob | `knob 1`: pitch tracks ANGLE within one tick | L1 |
| Greet once | Walk up: one chirp, then silence | L1 |
| Dead-man | Kill host mid-`run`: tach releases, face idle | L1 |
| Escape hatch | `repl` exits clean; redeploy with no hands | L1 |
| Wrong-sensor | `duty_pct 5` distinguishable on the tach edge | L1 |
| **The closer** | Xuss-C as fixture runs a sibling GCU metal gate green | **L2** |

## 9. The build (for the agent)

1. Read this file. Do not edit it.
2. **Investigate ESP32-Synth** (§3.1) before writing voice metal code.
3. Host L0: pure C under `host/` + CTest. Edge math and protocol must not require a board.
4. Metal: ESP-IDF app under `firmware/` (or `main/`), flash with esptool/idf.py.
5. Twin reference: [tig/xuss](https://github.com/tig/xuss) for product behavior; do not copy MicroPython modules blindly.
6. Ambiguity log in every PR.

## 10. Open items

- [ ] Servo pan/tilt head (v2)
- [ ] Forcing-channel harness for DUT switch inputs (v2)
- [ ] Current-sense day job (v2): AMeter on Port A
- [ ] Mic party trick (v3)
- [ ] Optional: silico host tooling bridge for C deploys (not required for L1)
