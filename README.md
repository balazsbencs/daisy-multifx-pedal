# Multi-FX

A stereo-out stompbox built on the [Daisy Seed](https://electro-smith.com/products/daisy-seed)
(STM32H750 + AKM AK4556) that chains three independent effect stages —
**Modulation → Delay → Reverb** — through a 240×320 colour ST7789 display
and dedicated bypass footswitches per stage.

```
    ┌─────────┐  ┌─────────┐   ┌──────────┐
IN ▶│   MOD   │▶─│  DELAY  │▶──│  REVERB  │▶ OUT
    └─────────┘  └─────────┘   └──────────┘
         ▲            ▲              ▲
         └─ 6 modes ──┴── 10 modes ──┴── 12 modes ──
```

- 28 effect algorithms total (6 modulation / 10 delay / 12 reverb)
- 7 parameters per stage, accessed via 4 encoders + shift
- Tap tempo footswitch with MIDI-clock priority arbitration
- Stereo audio path, mono input → stereo wet output
- MIDI in over USB and 5-pin DIN (UART)
- 100 presets in QSPI flash (10 banks × 10 slots) with live-state auto-save
- Companion Tauri desktop app for preset management over USB MIDI SysEx

---

## Project layout

```
src/
├── main.cpp                     ← top-level loop, UI dispatch
├── config/                      ← pin map, mode IDs, constants
├── hardware/                    ← controls (encoders, switches)
├── audio/                       ← audio_engine: 3-stage chain, mix coeffs
├── dsp/                         ← shared DSP blocks (delay line, LFO, FDN, filters, …)
├── modes/                       ← 28 effect algorithms + per-stage registries
├── params/                      ← typed param sets, ranges, mode→range maps
├── midi/                        ← UART + USB MIDI handler
├── tempo/                       ← tap-tempo + MIDI-clock arbitration
├── presets/                     ← QSPI-backed storage (100 presets + live state)
└── display/                     ← ST7789 SPI driver, renderer, layout, manager

editor/                          ← Tauri desktop app (preset editor, USB MIDI SysEx sync)

third_party/
├── libDaisy/                    ← BSP + HAL
└── DaisySP/                     ← reference DSP library
```

For day-to-day operation, see [docs/USER_MANUAL.md](docs/USER_MANUAL.md).
For wiring and the pin map, see [docs/HARDWARE.md](docs/HARDWARE.md).
For preset storage and the SysEx protocol, see [docs/PRESETS.md](docs/PRESETS.md) and [docs/MIDI_USB.md](docs/MIDI_USB.md).

---

## Building

### Prerequisites

- `arm-none-eabi-gcc` toolchain (10.3+)
- `make`
- A populated `third_party/libDaisy` and `third_party/DaisySP`
  (both ship as git submodules)

```sh
git submodule update --init --recursive
make -C third_party/libDaisy
make -C third_party/DaisySP
```

### Build

```sh
make            # produces build/multi-fx.{elf,bin,hex}
```

Output is built with `-Os -flto`.  The binary targets the QSPI flash
via the Daisy bootloader (`APP_TYPE = BOOT_QSPI`).

The Makefile pads the `.bin` to ≥131073 bytes so the last byte falls in a
DFU-writeable QSPI sector — without the pad, `dfu-util` aborts with
"Last page … is not writeable".

### Flash

The pedal expects the [Daisy Bootloader](https://github.com/electro-smith/DaisyBootloader)
to already be present (so `APP_TYPE=BOOT_QSPI` works).  To flash the bootloader
once, follow Electrosmith's instructions.

To flash the application over USB DFU, press RESET only (do **not** hold BOOT —
that enters ROM DFU which cannot write QSPI). Within 2 seconds run:

```sh
make program-dfu
```

Internally:

```sh
dfu-util -a 0 -s 0x90040000:leave -D build/multi-fx.bin -d ,0483:df11
```

---

## Display

The 240×320 ST7789 shows the active stage tab, current mode name, four parameter bars at a time (shift encoder to see the remaining three), and the chain strip at the bottom. The bottom row shows the current preset bank/slot (`B0 P00`–`B9 P09`).

| Normal view | Shift view (params 5–7) |
|:-----------:|:-----------------------:|
| ![Normal](docs/images/screen_normal.png) | ![Shifted](docs/images/screen_shifted.png) |

---

## Architecture at a glance

- **Audio:** `AudioEngine::ProcessBlock` runs at 48 kHz, block size 48
  (1 ms latency).  Three stages chained mono-in / stereo-out.  Per-stage
  bypass and constant-power wet/dry crossfade with cached coefficients.
- **Controls:** 4 quadrature encoders polled at 500 Hz from a TIM3 ISR
  (shift-register debounce, accumulator-per-detent).  The 5th encoder
  (mode) and the 4 footswitches use `daisy::Switch` debounce in the
  main loop.  Supported encoder types: Alps EC11 (2 transitions/detent)
  and Bourns PEC11R (4 transitions/detent) — see
  [Encoder hardware](#encoder-hardware) below.
- **Display:** ST7789 driven over SPI1 with DMA.  Frame buffer lives in
  SDRAM (240 × 320 × 16 bpp).  Updated at ~30 fps.
- **Concurrency:** main loop publishes `MultiParamBuf` via double-buffer
  (atomic flag flip in IRQ-blocking section).  Audio callback always
  reads the most recently published buffer.
- **Memory:** No heap allocation anywhere.  Long delay lines live in
  SDRAM; mod-FX delay lines fit in DTCMRAM.

---

## Encoder hardware

The four parameter encoders are driven by a type-aware quadrature decoder that
normalises the different pulse-per-detent counts of common encoder families.
Pass the matching `EncoderType` to `Controls::Init` in `src/main.cpp`:

| Encoder family  | Transitions / detent | `EncoderType` constant          |
|-----------------|---------------------|---------------------------------|
| Alps EC11       | 2                   | `Controls::EncoderType::ALPS_EC11` (default) |
| Bourns PEC11R   | 4                   | `Controls::EncoderType::BOURNS_PEC11R`        |

```cpp
// src/main.cpp — inside AppInit()
controls.Init(hw);                                               // Alps EC11 (default)
controls.Init(hw, pedal::Controls::EncoderType::BOURNS_PEC11R); // Bourns PEC11R
```

The `EncoderType` integer value is the threshold used by the half-step
accumulator: the decoder fires one step when it accumulates that many
quadrature transitions in one direction.  Adding support for a new encoder
family only requires adding a new enum value with the correct transition count.

---

## License

This project ships submodules under their own licenses
(libDaisy and DaisySP are MIT).  Application code in `src/` follows the
same MIT terms unless stated otherwise.
