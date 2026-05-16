---
title: Wiring
description: Step-by-step wiring checklist and suggested enclosure layout
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
