# Xuss-C

A bench drone that sings the engine it simulates — **C / ESP-IDF** edition.

Same product contract as [tig/xuss](https://github.com/tig/xuss) (MicroPython + silico). Xuss-C is the parallel proof that the instrument can ship as native firmware without abandoning the acceptance table.

The name is the short half of Turminder Xuss, the drone in Iain M. Banks' *Matter*.

## Status

**Spec + host skeleton.** [spec.md](spec.md) is the contract. Firmware is built from it by an agent, on a branch, with an ambiguity log.

- Product twin of **tig/xuss** (M5GO IoT Starter Kit v2.7, zero solder).
- Implementation language: **C** (ESP-IDF), not MicroPython.
- Voice path **must** investigate [The-Shreyas-M/ESP32-Synth](https://github.com/The-Shreyas-M/ESP32-Synth) before locking DAC/I2S design (see spec §3 and §9).

Background: [tig/silico#50](https://github.com/tig/silico/issues/50), twin [tig/xuss](https://github.com/tig/xuss).

## Host gate (now)

```text
cmake -S host -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Or: `cmake --build build/host --target test` after configure.
