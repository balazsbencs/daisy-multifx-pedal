---
title: Presets
description: 8-slot QSPI flash preset storage — current state and planned UI
---

## Presets

The firmware ships with a persistent storage layer in QSPI flash for
**8 preset slots** (slot label "P1".."P8" shown top-right of the
display). Each slot stores:

- The selected mode for each of the three stages
- All seven parameters per stage (21 floats total)
- A `valid` flag

The storage layer is initialised at boot and the slot label is rendered,
but **save / load UI bindings are not yet wired in this firmware
revision** — the slot stays at P1 and existing slots cannot yet be
recalled from the front panel. This is the next planned UI feature.

If you want to use presets today, drive `MultiPresetManager::SaveSlot`
and `LoadSlot` from custom code, or wait for the encoder long-press
binding.

## Power-on defaults

| Stage       | Mode    | Notable defaults                        |
|-------------|---------|-----------------------------------------|
| Modulation  | Chorus  | Speed ≈ 1.5 Hz, Depth 0.5, Mix 0.5     |
| Delay       | Tape    | Time ≈ 600 ms, Repeats 0.4, Mix 0.5    |
| Reverb      | Hall    | Decay short, Pre-delay 20 ms, Mix 0.5  |

All three stages are **bypassed** at power-up. Stomp the relevant
footswitch to engage.
