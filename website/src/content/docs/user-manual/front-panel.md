---
title: Front Panel
description: Controls, LEDs, and display layout of the Multi-FX pedal
---

## Front panel

| Control          | Function                                                       |
|------------------|----------------------------------------------------------------|
| **MOD** switch   | Toggle modulation stage on / off                               |
| **DLY** switch   | Toggle delay stage on / off                                    |
| **REV** switch   | Toggle reverb stage on / off                                   |
| **TAP** switch   | Tap tempo (short press) / hold (long press)                    |
| **MOD** LED      | Lit while MOD stage is engaged                                 |
| **DLY** LED      | Lit while DLY stage is engaged                                 |
| **REV** LED      | Lit while REV stage is engaged                                 |
| **Mode** encoder | Rotate: cycle modes within active page. Click: cycle pages. Hold + rotate: shift parameter encoders to params 5–7. |
| **P0..P3**       | Four parameter encoders — edit the four parameters shown for the active page. Hold the mode encoder to access params 5–7. |

---

## Display

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

- The **tab strip** at the top shows which effect page you're editing.
  Active tab is filled with the page accent colour
  (cyan = MOD, orange = DLY, blue = REV).
- The **mode name** is the algorithm currently selected for the active
  page.
- The **chain strip** at the bottom shows all three stages at once.
  A disabled stage is dimmed and crossed-out.
- **Status row:** "HOLD" appears in red while reverb hold is active;
  "SAVED" / "LOAD" / "ERR" briefly flashes after a preset operation.
