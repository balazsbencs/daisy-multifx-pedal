---
title: MIDI
description: USB and DIN MIDI — CC map, clock sync, and realtime messages
---

The pedal listens on **USB MIDI** (always) and **5-pin DIN UART MIDI**
(if you've wired the alternate hardware path).

## Control Change (CC)

Each parameter is mapped to a CC. Values 0–127 are normalised to
[0, 1] and overwrite the corresponding parameter:

| CC range  | Stage              |
|-----------|--------------------|
| 14 – 20   | Modulation params  |
| 21 – 27   | Delay params       |
| 28 – 34   | Reverb params      |
| 65        | Reverb hold (0/1)  |

CC 65 ≥ 64 → hold ON, < 64 → hold OFF.

## Realtime

| Message      | Effect                                  |
|--------------|-----------------------------------------|
| `0xF8` Clock | Drive tempo (24 ppq, 2 s timeout)       |
| `0xFC` Stop  | Release tempo override                  |

## Program Change

The handler decodes the byte but the main loop currently ignores it
(reserved for preset recall — see the [Presets](/user-manual/presets/) page).
