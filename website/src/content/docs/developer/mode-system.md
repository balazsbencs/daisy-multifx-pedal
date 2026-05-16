---
title: Mode System
description: Abstract base classes, virtual interface, ModeRegistry, and mode switching
---

Each stage has an abstract base (`ModMode` / `DelayMode` / `ReverbMode`) with four virtual methods:
- `Init()` — once at startup
- `Reset()` — on mode switch (clears buffers/state)
- `Prepare(const ParamSet&)` — optional per-block setup (e.g. filter coefficient update)
- `Process(input, const ParamSet&) → StereoFrame` — per-sample DSP, returns wet-only

Each stage has a `ModeRegistry` owning all statically-allocated mode instances. Mode switching is a pointer swap + `Reset()` — no heap allocation, no audio dropout beyond the state clear.
