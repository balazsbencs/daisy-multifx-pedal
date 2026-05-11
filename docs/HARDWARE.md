# Hardware Guide

Authoritative wiring for the Multi-FX pedal. The single source of truth
is `src/config/pin_map.h` — this document mirrors it and adds wiring
notes, BOM, and physical layout.

If you change a pin in `pin_map.h`, update this file in the same commit.

---

## Daisy Seed pin assignments

All pins below are Daisy Seed labels (D0–D30, plus power/audio). The
underlying STM32H750 pin is annotated where it matters (SPI alternate
function on the display).

### Encoders (5 total)

| Function           | Daisy pin | Notes                              |
|--------------------|-----------|------------------------------------|
| Mode encoder A     | D0        | Quadrature, internal pull-up       |
| Mode encoder B     | D1        | Quadrature, internal pull-up       |
| Mode encoder SW    | D2        | Push-switch, internal pull-up      |
| Param encoder 0 A  | D7        | Quadrature, polled at 500 Hz (ISR) |
| Param encoder 0 B  | D8        |                                    |
| Param encoder 1 A  | D9        |                                    |
| Param encoder 1 B  | D10       |                                    |
| Param encoder 2 A  | D27       |                                    |
| Param encoder 2 B  | D28       |                                    |
| Param encoder 3 A  | D29       |                                    |
| Param encoder 3 B  | D30       |                                    |

All encoder A/B pins use the MCU's internal pull-up. Wire each encoder
common pin to **GND**. Recommended part: **Alps EC11** (24 pulses,
24 detents). The decoder treats two transitions as one detent
(`accum_ ±2`), so any standard Alps-compatible 11mm encoder works.

The mode encoder uses libDaisy's blocking debounced encoder
(`daisy::Encoder`); the four parameter encoders use a custom
shift-register debouncer driven from a 500 Hz TIM3 ISR — this catches
fast turns that the main-loop poll would miss while audio + display
work is in flight.

### Footswitches (4 total)

| Function            | Daisy pin | Wiring                          |
|---------------------|-----------|---------------------------------|
| FX 1 (Modulation)   | D3        | SPST momentary, NO, to GND      |
| FX 2 (Delay)        | D4        | SPST momentary, NO, to GND      |
| FX 3 (Reverb)       | D11       | SPST momentary, NO, to GND      |
| Tap / Hold          | D12       | SPST momentary, NO, to GND      |

Switches use libDaisy's `daisy::Switch` with internal pull-up: connect
one terminal to the Daisy pin, the other to **GND**.

Use **soft-touch momentary** stomp switches (e.g. PEC-12, Boss-style).
The firmware is edge-triggered for FX bypass and tap; latching switches
will misbehave.

### Status LEDs (3)

| Function            | Daisy pin | Drive                                            |
|---------------------|-----------|--------------------------------------------------|
| FX 1 LED (MOD)      | D6        | Active-high, ~330 Ω series resistor to LED → GND |
| FX 2 LED (DELAY)    | D15       | "                                                |
| FX 3 LED (REVERB)   | D16       | "                                                |

Daisy GPIOs source ~2 mA cleanly. Use a low-current LED, or buffer with
a transistor if you want a high-brightness indicator. Each LED tracks
its corresponding stage's bypass state (lit = engaged).

### ST7789 colour display (240×320, SPI1)

| Function             | Daisy pin | STM32 pin | Notes                            |
|----------------------|-----------|-----------|----------------------------------|
| SCK                  | D22       | PA5       | SPI1_SCK, alternate function     |
| MOSI / SDA           | D18       | PA7       | SPI1_MOSI, alternate function    |
| CS                   | D13       | PB6       | GPIO output                      |
| DC                   | D14       | PB7       | GPIO output (data/command)       |
| RES                  | D26       | PD11      | GPIO output (active-low reset)   |
| BLK / Backlight      | D24       | PA1       | GPIO output, drive HIGH for ON   |

The display is driven over **SPI1 + DMA** — the 240×320×2-byte frame
buffer lives in SDRAM and is pushed in one transfer per frame.

> **Important conflict note:** libDaisy's default UART MIDI handler
> reuses **D13 / D14** for its TX/RX before reconfiguration. Initialise
> the MIDI handler **before** the display, otherwise the display CS/DC
> pins end up in UART alternate-function mode and the screen stays
> blank. This ordering is enforced in `main.cpp`.

### MIDI

| Function            | Underlying interface                   |
|---------------------|----------------------------------------|
| MIDI USB            | Daisy's native USB device port         |
| MIDI UART RX (D14¹) | UART1 RX (libDaisy default)            |
| MIDI UART TX (D13¹) | UART1 TX (libDaisy default — unused)   |

¹ Pins are claimed by the display **after** MIDI init. The UART path is
compiled in but the pins are repurposed for SPI by the time audio runs.
If you need 5-pin DIN MIDI, route MIDI before the display init, wire a
standard opto-isolated MIDI input circuit (6N138 or H11L1) to the UART
RX pin, and accept that DIN MIDI and the on-board display cannot coexist
with the current pin choice. **USB MIDI works unconditionally.**

### Audio I/O (on-board AK4556 codec)

The Daisy Seed exposes the codec on its dedicated audio pads. No extra
wiring is required beyond the standard reference topology:

- **Audio In L/R:** AC-couple the input through a 1 µF cap to the Seed's
  AUDIO IN L/R pads, biased to AGND through 47 kΩ. This pedal uses a
  mono input (left channel — the audio engine duplicates it before
  stage 1).
- **Audio Out L/R:** AC-couple AUDIO OUT L/R through a 10 µF cap and a
  small series resistor (e.g. 100 Ω) to the output jacks. The stereo
  spread comes from the wet path of every effect.

For a TS-input / TRS-output guitar pedal, follow the Electrosmith
[Pod reference schematic](https://github.com/electro-smith/Hardware/tree/master/reference)
for the analog front end.

### Power

- **5 V via USB** — sufficient for development.
- **Stompbox installation:** 9 V centre-negative DC jack →
  9 V→5 V buck regulator → Daisy's `+5V` pad. The Daisy's onboard
  3.3 V LDO supplies digital + display rails.
- **Backlight** draws the most current of any peripheral (≈ 30 mA at
  full brightness). Budget 200 mA total at 5 V.

### Unassigned pins

- **D5** is reserved (originally a relay-bypass control); free for
  expansion (e.g. a toggle switch, a true-bypass relay coil driver, or
  an expression pedal jack tip-sense).

---

## Physical layout (suggested enclosure)

```
┌──────────────────────────────────────────────────┐
│              ST7789  240 × 320 display           │
│  (3 vertical pages: MOD / DLY / REV)             │
│                                                  │
│   ┌──┐ ┌──┐ ┌──┐ ┌──┐                            │
│   │M │ │P0│ │P1│ │P2│ ┌──┐    ← top row          │
│   │OD│ │  │ │  │ │  │ │P3│      M = mode encoder │
│   └──┘ └──┘ └──┘ └──┘ └──┘      P0..P3 = params  │
│                                                  │
│   ●        ●        ●           ← LEDs (per FX)  │
│   ◉        ◉        ◉   ◉       ← footswitches   │
│   MOD     DLY     REV   TAP                      │
└──────────────────────────────────────────────────┘
   ┌─────┐                  ┌─────┐ ┌─────┐
   │ IN  │                  │OUT L│ │OUT R│
   └─────┘                  └─────┘ └─────┘
   (TS)                     (TS each — stereo)
```

The display is portrait orientation: tabs at top, parameter rows in the
middle, status at bottom, full effect-chain strip at the very bottom.

---

## Bill of materials (essentials)

| Qty | Part                            | Notes                                          |
|-----|---------------------------------|------------------------------------------------|
| 1   | Electrosmith Daisy Seed         | rev 1.x with AK4556 codec                      |
| 1   | ST7789 240×320 SPI display      | IPS preferred, 7-pin breakout                  |
| 5   | Alps EC11 24-detent encoder     | mode encoder needs the integrated push switch  |
| 4   | SPST momentary stomp switch     | NO, soft-touch                                 |
| 3   | LED + 330 Ω resistor            | 3 mm, low-current                              |
| 1   | DC jack, 9 V centre-negative    | barrel 2.1 mm                                  |
| 1   | 9V → 5V buck (Pololu D24V6F5)   | 500 mA out                                     |
| 2   | 1/4" TS jacks (output)          | stereo wet                                     |
| 1   | 1/4" TS jack (input)            | mono                                           |
| 1   | 5-pin DIN jack (optional)       | for hardware MIDI in                           |
| —   | 1 µF / 10 µF coupling caps      | analog front-end                               |
| —   | 47 kΩ bias resistors            | input AGND bias                                |
| —   | Knobs (5 push-on)               | per encoder                                    |

---

## Quick wiring checklist

1. **Encoders:** A / B / SW → Daisy pin, common → GND.
2. **Footswitches:** one terminal → Daisy pin, other → GND.
3. **LEDs:** Daisy pin → 330 Ω → LED anode, cathode → GND.
4. **Display:**
   - VCC → 3.3 V, GND → GND
   - SCK → D22, SDA → D18
   - CS → D13, DC → D14
   - RES → D26, BLK → D24
5. **Audio:** input L only, outputs L + R, AC-coupled.
6. **Power:** 5 V to Daisy `+5V` from the buck regulator.
7. **MIDI USB:** plug any USB MIDI host into the Seed's USB port — no
   wiring required.

When in doubt, defer to `src/config/pin_map.h` — that file is the
reference, this document mirrors it.
