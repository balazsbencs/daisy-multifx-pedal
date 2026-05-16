---
title: Parameters
description: Typed ParamSets, normalization pipeline, ParamRange, and map_param
---

## Parameter Pipeline

Each stage has 7 parameters flowing as normalized `[0, 1]` values, converted to physical units at the last moment:

```
Encoders / MIDI CC / Preset recall
        ↓
  main loop normalization arrays (mod_norm / delay_norm / reverb_norm)
        ↓
  BuildParams() → map_param() → typed ParamSet  (physical units, immutable snapshot)
        ↓
  TempoSync override (clamps time/speed when tap or MIDI clock active)
        ↓
  AudioEngine::SetParams()  (writes to idle double-buffer via MultiParamBuf)
        ↓
  AudioCallback ISR → Mode::Process()
```

`ParamSet` is an immutable snapshot — never mutate it, always produce a new one. `ParamRange` defines `{min, max, curve}` per parameter; `map_param()` / `unmap_param()` convert between normalized and physical.

## Source layout

Parameters live in `src/params/`:

| File | Responsibility |
|------|---------------|
| `mod_params.h/.cpp` | `ModParamSet` struct, `BuildModParams()`, range definitions for all 6 mod modes |
| `delay_params.h/.cpp` | `DelayParamSet` struct, `BuildDelayParams()`, range definitions for all 4 delay modes |
| `reverb_params.h/.cpp` | `ReverbParamSet` struct, `BuildReverbParams()`, range definitions for all 4 reverb modes |
| `param_range.h` | `ParamRange` struct and `map_param()` / `unmap_param()` free functions |

## ParamRange and map_param

`ParamRange` stores three fields:

```cpp
struct ParamRange {
    float min;
    float max;
    Curve curve;   // LINEAR, SQUARE, SQRT, LOG, EXP
};
```

`map_param(norm, range)` converts a normalized `[0, 1]` float to the physical unit using the specified curve. `unmap_param(physical, range)` is the inverse — used when loading presets to restore the normalized value.

The `SQUARE` curve concentrates resolution at the low end (used for delay TIME so short delays have fine control), while `LOG` concentrates resolution at the high end.

## Normalization arrays

The main loop maintains three float arrays:

```
mod_norm[7]    — normalized [0,1] values for the active mod mode
delay_norm[7]  — normalized [0,1] values for the active delay mode
reverb_norm[7] — normalized [0,1] values for the active reverb mode
```

Encoder steps increment / decrement the relevant index (clamped to `[0, 1]`). MIDI CC writes go directly to the same arrays. Preset load populates all three arrays at once via `unmap_param`.

`BuildParams()` is called once per main-loop iteration, constructing fresh `ParamSet` snapshots from the current normalization arrays. The snapshots are then handed to `AudioEngine::SetParams()`.

## Immutability rule

`ParamSet` structs are **read-only** after construction. They must never be modified in place. `Mode::Process()` receives a `const ParamSet&` and may only read from it. Any update to a parameter must flow through the normalization arrays → `BuildParams()` cycle.
