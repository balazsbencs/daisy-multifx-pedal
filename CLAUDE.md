# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

First-time only — build the libraries:
```bash
git submodule update --init --recursive
make -C third_party/libDaisy
make -C third_party/DaisySP
```

Incremental build:
```bash
make -j4
# Output: build/multi-fx.{elf,bin,hex}
```

Flash to hardware (requires Daisy Bootloader already on device):
```bash
make program-dfu   # USB DFU: hold BOOT, press RESET, then run this
```

There are no automated tests in this project.

## Architecture

### Signal Chain

Audio flows mono-in → stereo-out through three chained stages:

```
IN → [MOD stage] → [DELAY stage] → [REVERB stage] → OUT
       6 modes        10 modes         9 modes
```

Each stage has independent bypass (footswitch + LED) and constant-power wet/dry crossfade. Modes output **wet signal only** — the audio engine applies dry/wet mixing.

### Parameter Pipeline

Each stage has 7 parameters flowing as normalized `[0, 1]` values, converted to physical units at the last moment:

```
Encoders / MIDI CC / Preset recall
        ↓
  main loop normalization arrays (mod_norm / delay_norm / reverb_norm)
        ↓
  BuildParams() → map_param() → typed ParamSet  (physical units, immutable snapshot)
        ↓
  TempoSync override (clamps time/speed when tap or MIDI clock active)
        ↓
  AudioEngine::SetParams()  (writes to idle double-buffer via MultiParamBuf)
        ↓
  AudioCallback ISR → Mode::Process()
```

`ParamSet` is an immutable snapshot — never mutate it, always produce a new one. `ParamRange` defines `{min, max, curve}` per parameter; `map_param()` / `unmap_param()` convert between normalized and physical.

### Mode System

Each stage has an abstract base (`ModMode` / `DelayMode` / `ReverbMode`) with four virtual methods:
- `Init()` — once at startup
- `Reset()` — on mode switch (clears buffers/state)
- `Prepare(const ParamSet&)` — optional per-block setup (e.g. filter coefficient update)
- `Process(input, const ParamSet&) → StereoFrame` — per-sample DSP, returns wet-only

Each stage has a `ModeRegistry` owning all statically-allocated mode instances. Mode switching is a pointer swap + `Reset()` — no heap allocation, no audio dropout beyond the state clear.

### Source Layout

```
src/
├── main.cpp               ← top-level init + main loop (controls, display, MIDI dispatch)
├── config/                ← pin_map.h, mode IDs, build constants
├── hardware/              ← encoder/footswitch polling (ISR at 500 Hz + debounce)
├── audio/                 ← AudioEngine: 3-stage chain, MultiParamBuf double-buffer
├── dsp/                   ← shared DSP blocks: delay line, FDN, LFO, SVF, allpass,
│                             envelope follower, pitch shifter, diffuser, saturation, etc.
├── modes/                 ← 14 effect algorithms + per-stage registries
├── params/                ← typed param sets + normalization maps per stage
├── midi/                  ← USB + UART MIDI handler (CC + clock)
├── tempo/                 ← tap-tempo + MIDI-clock arbitration
├── presets/               ← QSPI-backed persistent storage (8 slots)
└── display/               ← ST7789 SPI/DMA driver, renderer, layout manager
```

### SDRAM Allocation

Long delay lines use `DSY_SDRAM_BSS` (placed in external SDRAM by the linker). These must be **file-scope static** — never stack or heap. Modulation-FX delay lines fit in DTCMRAM. The display frame buffer (240×320×2 bytes) also lives in SDRAM.

### Thread Safety

The audio ISR and main loop share state only via `MultiParamBuf` — a double-buffered struct in `AudioEngine`. The main loop writes to the idle buffer and sets an atomic flag in an IRQ-blocking section; the ISR swaps the read index at block entry. No mutexes needed. Never read `MultiParamBuf` from the main loop after publishing.

### Display Init Order

`MIDI UART (D13/D14) must be initialized before the ST7789 display` — the display uses the same pin numbers for CS/DC, and initializing MIDI last would overwrite the display configuration.

### Tempo Priority Chain

`TempoSync` arbitrates the `time`/`speed` parameter across all three stages:
1. **MIDI Clock** (highest) — locks to beat period; expires 2 s after last tick
2. **Tap Tempo** — averaging up to 4 taps; 2 s timeout
3. **Encoder / MIDI CC** — normal control

### Flash / Memory Budget

Binary built with `-Os -flto` targeting QSPI flash via the Daisy bootloader (`APP_TYPE=BOOT_QSPI`). The Makefile pads the `.bin` to ≥131,073 bytes to avoid a `dfu-util` QSPI sector alignment abort. No heap allocation anywhere in the firmware.

### Documentation

- `docs/HARDWARE.md` — pin map, wiring, encoder/display/MIDI connections
- `docs/USER_MANUAL.md` — front-panel controls, display layout, MIDI CC table
- `README.md` — project overview and build instructions
