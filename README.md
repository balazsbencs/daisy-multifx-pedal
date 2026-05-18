# Multi-FX

A stereo-out stompbox built on the [Daisy Seed](https://electro-smith.com/products/daisy-seed)
(STM32H750 + AKM AK4556) that chains three independent effect stages —
**Modulation → Delay → Reverb** — through a 240×320 colour ST7789 display
and dedicated bypass footswitches per stage.

```
    ┌─────────┐  ┌─────────┐   ┌──────────┐
IN ▶│   MOD   │▶─│  DELAY  │▶──│  REVERB  │▶ OUT
    └─────────┘  └─────────┘   └──────────┘
         ▲            ▲             ▲
         └─ 6 modes ──┴── 4 modes ──┴── 4 modes ──
```

- 14 effect algorithms total (6 modulation / 4 delay / 4 reverb)
- 7 parameters per stage, accessed via 4 encoders + shift
- Tap tempo footswitch with MIDI-clock priority arbitration
- Stereo audio path, mono input → stereo wet output
- MIDI in over USB and 5-pin DIN (UART)
- Preset infrastructure in QSPI flash (8 slots)
- Long-press tap → infinite reverb hold (freeze)

---

## Project layout

```
src/
├── main.cpp                     ← top-level loop, UI dispatch
├── config/                      ← pin map, mode IDs, constants
├── hardware/                    ← controls (encoders, switches)
├── audio/                       ← audio_engine: 3-stage chain, mix coeffs
├── dsp/                         ← shared DSP blocks (delay line, LFO, FDN, filters, …)
├── modes/                       ← 14 effect algorithms + per-slot registries
├── params/                      ← typed param sets, ranges, mode→range maps
├── midi/                        ← UART + USB MIDI handler
├── tempo/                       ← tap-tempo + MIDI-clock arbitration
├── presets/                     ← QSPI-backed persistent storage (8 slots)
└── display/                     ← ST7789 SPI driver, renderer, layout, manager

third_party/
├── libDaisy/                    ← BSP + HAL
└── DaisySP/                     ← reference DSP library
```

For day-to-day operation, see [docs/USER_MANUAL.md](docs/USER_MANUAL.md).
For wiring and the pin map, see [docs/HARDWARE.md](docs/HARDWARE.md).

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

To flash the application over USB DFU (Daisy in bootloader mode — hold BOOT,
press RESET):

```sh
make program-dfu
```

Internally:

```sh
dfu-util -a 0 -s 0x90040000:leave -D build/multi-fx.bin -d ,0483:df11
```

---

## Architecture at a glance

- **Audio:** `AudioEngine::ProcessBlock` runs at 48 kHz, block size 48
  (1 ms latency).  Three stages chained mono-in / stereo-out.  Per-stage
  bypass and constant-power wet/dry crossfade with cached coefficients.
- **Controls:** 4 quadrature encoders polled at 500 Hz from a TIM3 ISR
  (shift-register debounce, accumulator-per-detent).  The 5th encoder
  (mode) and the 4 footswitches use `daisy::Switch` debounce in the
  main loop.
- **Display:** ST7789 driven over SPI1 with DMA.  Frame buffer lives in
  SDRAM (240 × 320 × 16 bpp).  Updated at ~30 fps.
- **Concurrency:** main loop publishes `MultiParamBuf` via double-buffer
  (atomic flag flip in IRQ-blocking section).  Audio callback always
  reads the most recently published buffer.
- **Memory:** No heap allocation anywhere.  Long delay lines live in
  SDRAM; mod-FX delay lines fit in DTCMRAM.

---

## License

This project ships submodules under their own licenses
(libDaisy and DaisySP are MIT).  Application code in `src/` follows the
same MIT terms unless stated otherwise.
