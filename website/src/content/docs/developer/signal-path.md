---
title: Signal Path
description: Parameter pipeline from encoder input to audio callback
---

## Signal Chain

Audio flows mono-in → stereo-out through three chained stages:

```
IN → [MOD stage] → [DELAY stage] → [REVERB stage] → OUT
       6 modes        10 modes         9 modes
```

Each stage has independent bypass (footswitch + LED) and constant-power wet/dry crossfade. Modes output **wet signal only** — the audio engine applies dry/wet mixing.

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
