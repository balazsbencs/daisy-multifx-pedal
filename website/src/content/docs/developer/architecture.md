---
title: Architecture
description: High-level architecture of the Multi-FX firmware — audio, controls, display, concurrency
---

Multi-FX is a bare-metal C++ firmware for the STM32H750 (via Daisy Seed). There is no OS —
the system has two execution contexts: the **main loop** and the **audio ISR**.

## Audio engine

`AudioEngine::ProcessBlock` runs at 48 kHz with a block size of 48 samples (1 ms latency).
Three effect stages are chained mono-in / stereo-out:

```
IN → [MOD stage] → [DELAY stage] → [REVERB stage] → OUT
       6 modes        10 modes         9 modes
```

Each stage has:
- Independent bypass (footswitch + LED) — bypassed stage passes audio through with zero DSP work
- Constant-power wet/dry crossfade (sin/cos equal-power curves, unity-normalised)
- Modes output **wet signal only** — the engine applies the dry/wet mix

## Controls

- 4 quadrature parameter encoders polled at 500 Hz from a TIM3 ISR (shift-register debounce,
  accumulator-per-detent)
- Mode encoder + 4 footswitches use `daisy::Switch` debounce in the main loop
- Fast-spin detection: two detents within 40 ms → 5% step instead of 1%

## Display

- ST7789 240×320 driven over SPI1 with DMA
- Frame buffer (240×320×2 bytes = 150 kB) lives in SDRAM
- Updated at ~30 fps from the main loop
- Three-tab layout: MOD (cyan) / DLY (orange) / REV (blue)

## Concurrency model

The audio ISR and main loop share state **only** via `MultiParamBuf` — a double-buffered struct in
`AudioEngine`:

1. Main loop writes to the **idle** buffer
2. Main loop sets an atomic flag in an IRQ-blocking section
3. Audio ISR swaps the read index at block entry

No mutexes needed. The main loop never reads `MultiParamBuf` after publishing.

## Memory layout

| Region | Contents |
|--------|---------|
| DTCMRAM | Stack, fast code, modulation delay lines |
| AXI SRAM | Main data, `AudioEngine`, most state |
| SDRAM | Long delay lines (`DSY_SDRAM_BSS`), display frame buffer |
| QSPI flash | Firmware image (via Daisy bootloader), preset storage |

All SDRAM allocations use `DSY_SDRAM_BSS` and must be **file-scope static** — never stack or heap.

**No heap allocation anywhere in the firmware.**

## Source layout

```
src/
├── main.cpp          top-level init + main loop
├── config/           pin_map.h, mode IDs, build constants
├── hardware/         encoder/footswitch polling (ISR + debounce)
├── audio/            AudioEngine, MultiParamBuf double-buffer
├── dsp/              shared DSP blocks (see DSP Blocks)
├── modes/            14 effect algorithms + per-stage registries
├── params/           typed ParamSets + normalization maps
├── midi/             USB + UART MIDI handler (CC + clock)
├── tempo/            tap-tempo + MIDI-clock arbitration
├── presets/          QSPI-backed persistent storage (8 slots)
└── display/          ST7789 SPI/DMA driver, renderer, layout manager
```
