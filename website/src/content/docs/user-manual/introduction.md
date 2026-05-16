---
title: Introduction
description: Overview of the Multi-FX Daisy Seed stereo effects pedal
---

Multi-FX is a stereo-out stompbox built on the [Daisy Seed](https://electro-smith.com/products/daisy-seed)
(STM32H750 + AKM AK4556). It chains three independent effect stages in series:

```
IN ▶ MOD ▶ DELAY ▶ REVERB ▶ OUT (stereo)
```

Each stage has a dedicated **footswitch** (toggle on / off) and a **status LED** (lit = engaged).
On power-up all three stages are **bypassed** — your input passes dry to the output until you
tap a footswitch.

## Effect stages

| Stage | Algorithms | Description |
|-------|-----------|-------------|
| **MOD** | 6 | Chorus, Flanger, Rotary, Vibe, Phaser, Vintage Tremolo |
| **DELAY** | 4 | Digital, Tape, Dual, Filter-feedback |
| **REVERB** | 4 | Room, Hall, Plate, Spring |

## Hardware

- **MCU:** STM32H750 via Daisy Seed, running at 480 MHz
- **Codec:** AKM AK4556 — 48 kHz, 24-bit, stereo
- **Display:** 240×320 ST7789 colour TFT (SPI + DMA)
- **Controls:** 5 quadrature encoders (1 mode + 4 parameter), 4 footswitches, 3 status LEDs
- **MIDI:** USB MIDI (always on) + 5-pin DIN UART (hardware path)
- **Storage:** 8 preset slots in QSPI flash (64 MB)
- **Audio path:** mono input → stereo wet output (48 kHz / 48-sample blocks = 1 ms latency)

## Power-on defaults

| Stage | Mode | Notable defaults |
|-------|------|-----------------|
| Modulation | Chorus | Speed ≈ 1.5 Hz, Depth 0.5, Mix 0.5 |
| Delay | Tape | Time ≈ 600 ms, Repeats 0.4, Mix 0.5 |
| Reverb | Hall | Decay short, Pre-delay 20 ms, Mix 0.5 |

All three stages are **bypassed** at power-up. Stomp the footswitch to engage.

## Where to go next

- **Working the controls** → [Front Panel](./front-panel/)
- **Effect algorithms** → [Effect Modes](./effect-modes/)
- **MIDI control** → [MIDI](./midi/)
- **Building from source** → [Building & Flashing](../developer/building/)
- **Hardware wiring** → [Pin Map](../hardware/pin-map/)
