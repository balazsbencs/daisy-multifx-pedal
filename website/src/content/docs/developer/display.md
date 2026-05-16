---
title: Display
description: ST7789 SPI/DMA driver, display manager, init ordering constraint
---

## Display Init Order

`MIDI UART (D13/D14) must be initialized before the ST7789 display` — the display uses the same pin numbers for CS/DC, and initializing MIDI last would overwrite the display configuration.

## Display subsystem files

| File | Responsibility |
|------|---------------|
| `st7789_driver.h/.cpp` | Low-level SPI + DMA transfer, init sequence |
| `display_renderer.h/.cpp` | Drawing primitives: rect, text, bar, progress |
| `display_layout.h` | Layout constants (tab positions, row heights, colours) |
| `display_manager.h/.cpp` | High-level state machine: tab switching, param row updates, status row |
| `display_colors.h` | Colour constants matching the hardware display palette |

## Init ordering constraint

MIDI UART must be initialised **before** the ST7789 display. The display CS/DC pins (D13/D14)
overlap with the UART TX/RX alternate-function assignment. Initialising MIDI after the display
overwrites the SPI pin config and leaves the screen blank. This ordering is enforced in `main.cpp`.

## Hardware details

The ST7789 is driven over SPI1 with DMA transfers. The frame buffer (240 × 320 × 16 bpp = 150 KB)
lives in SDRAM (`DSY_SDRAM_BSS`) — it is too large for the internal SRAM regions. Updates run at
approximately 30 fps from the main loop.

## Screen layout

```
┌──────────────────────────────────────────┐
│  [ MOD ] [ DLY ] [ REV ]                 │  ← page tabs (active highlighted)
│                                          │
│  Tape                       P1           │  ← active mode + preset slot
│ ──────────────────────────────────────── │
│  TIME       ████████░░░░░░               │
│  REPEATS    █████░░░░░░░░░░              │
│  MIX        ███████░░░░░░░░              │
│  FILTER     ████░░░░░░░░░░░░             │  ← seven parameter rows
│  GRIT       ██░░░░░░░░░░░░░░             │
│  MOD SPD    ███░░░░░░░░░░░░░             │
│  MOD DEP    █░░░░░░░░░░░░░░░             │
│ ──────────────────────────────────────── │
│  HOLD     120 BPM       SAVED            │  ← status row
│ ──────────────────────────────────────── │
│  MOD: Chorus  >  DLY: Tape  >  REV: Hall │  ← chain strip
└──────────────────────────────────────────┘
```

- **Tab strip** — active page is filled with the accent colour (cyan = MOD, orange = DLY, blue = REV).
- **Parameter rows** — each of the 7 parameters is rendered as a label + progress bar. The bar is
  redrawn whenever the corresponding normalization value changes.
- **Status row** — shows the active tempo in BPM (tap or MIDI clock), the reverb hold indicator
  ("HOLD" in red when active), and a brief flash of "SAVED" / "LOAD" / "ERR" after preset operations.
- **Chain strip** — always shows all three stages. A bypassed stage is dimmed.

## Display manager state machine

`DisplayManager` tracks:

1. **Active page** — which of MOD / DLY / REV is currently shown. Tab switching redraws only the tab
   strip and the parameter rows; the chain strip is always current.
2. **Param dirty flags** — a bitmask per page. The main loop sets a flag when a normalization value
   changes; the display manager redraws only affected rows on the next frame.
3. **Status messages** — time-limited text overlays in the status row. The manager ages them out after
   a configurable number of frames.
