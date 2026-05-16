---
title: Parameters
description: The seven parameters per stage and how to access them with the encoders
---

## Parameters

Each page exposes seven parameters. The four physical encoders (P0–P3)
edit either parameters 1–4 (default) or parameters 5–7 (while you
hold down the mode encoder).

| Encoder        | Default param         | Shifted (mode held) |
|----------------|-----------------------|---------------------|
| P0             | param 1               | param 5             |
| P1             | param 2               | param 6             |
| P2             | param 3               | param 7             |
| P3             | param 4               | (unmapped)          |

Encoder behaviour:

- One detent = one step.
- Two detents within 40 ms = "fast mode" → step size jumps from
  1 % to 5 % per detent. Spin quickly to sweep, dial slowly to fine-tune.
- All parameters are clamped to [0, 1] internally and mapped to their
  per-mode physical range (e.g. delay TIME maps to 60 ms..2.5 s with a
  square-curve so the low end has more resolution).

### Modulation parameters

| Index | Label | Range / meaning                                   |
|-------|-------|---------------------------------------------------|
| 1     | SPEED | LFO rate, 0.05–10 Hz (square-curve)               |
| 2     | DEPTH | Modulation depth                                  |
| 3     | MIX   | Wet / dry                                         |
| 4     | TONE  | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)           |
| 5     | P1    | Mode-specific (e.g. feedback on Flanger/Vibe)     |
| 6     | P2    | Mode-specific                                     |
| 7     | LEVEL | Output gain, 0–2× (unity at 1.0)                  |

### Delay parameters

| Index | Label    | Range / meaning                                 |
|-------|----------|-------------------------------------------------|
| 1     | TIME     | Delay time, 60 ms – 2.5 s                       |
| 2     | REPEATS  | Feedback, 0 – 0.98                              |
| 3     | MIX      | Wet / dry                                       |
| 4     | FILTER   | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)         |
| 5     | GRIT     | Saturation / dirt amount                        |
| 6     | MOD SPD  | Modulation rate, 0.05–10 Hz                     |
| 7     | MOD DEP  | Modulation depth                                |

### Reverb parameters

| Index | Label    | Range / meaning                                       |
|-------|----------|-------------------------------------------------------|
| 1     | DECAY    | Reverb time, range varies per algo (0.2 s – 50 s)     |
| 2     | PRE DLY  | Pre-delay 0–500 ms                                    |
| 3     | MIX      | Wet / dry                                             |
| 4     | TONE     | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)               |
| 5     | MOD      | Modulation amount (chorus on the tail)                |
| 6     | PARAM1   | Per-algorithm                                         |
| 7     | PARAM2   | Per-algorithm                                         |
