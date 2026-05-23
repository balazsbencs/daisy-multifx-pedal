# DSP Quality Improvements Design

**Date**: 2026-05-23  
**Branch**: `feature/dsp-quality-v2` (from `main`)  
**Priority**: Sound quality above all else. Push CPU as needed; CPU meter provides visibility.  
**Stage priority**: Modulation → Delay → Reverb

---

## Goals

Raise the sonic ceiling of the multi-fx pedal across all three stages without targeting specific artifacts — a general quality upgrade. The pitch shifter (used in Shimmer reverb and Detune chorus) is secondary and excluded from this sprint.

---

## Section 1: Infrastructure — CPU Meter

### Measurement

Use the STM32H750 DWT cycle counter to measure cycles consumed inside `AudioCallback`:

- Read `DWT->CYCCNT` on entry and exit of the ISR.
- `cycles_per_block = CPU_FREQ * BLOCK_SIZE / SAMPLE_RATE` (constant).
- `cpu_usage = (exit_cycles - entry_cycles) / cycles_per_block`, clamped to [0, 1].
- Write result to a `volatile float cpu_usage_atomic` accessible from the main loop.

### Display

- Always visible in a fixed corner of the ST7789 display (integrated into existing layout, not an overlay).
- Main loop reads `cpu_usage_atomic` once per 1–2 seconds (~187–375 audio blocks) by averaging accumulated readings, then latches for display.
- Shows a horizontal bar + numeric percentage (e.g. `CPU 43%`).
- `DisplayManager` gains a `SetCpuUsage(float)` method called from the main loop.

### Data flow

```
AudioCallback ISR
  → measure DWT cycles
  → write volatile float cpu_usage_atomic

Main loop (~30 Hz display refresh)
  → accumulate cpu_usage_atomic over ~187 blocks
  → average → latch displayed value
  → DisplayManager::SetCpuUsage(averaged_value)
```

---

## Section 2: Modulation Stage

### 2a. Upgrade ToneFilter to 2nd-Order Biquad

**Current state**: Dual 1st-order IIR (one LP + one HP), -6 dB/oct slopes, no resonance.  
**Target**: Direct Form II transposed biquad, -12 dB/oct slopes, optional resonance (Q control).

**Changes**:
- Add biquad coefficient computation to `ToneFilter` (or replace the class body).
- Expose `SetBiquad(float cutoff_hz, float q, float sample_rate)` alongside existing interface.
- All modes currently using `ToneFilter` adopt the new path with Q defaulting to 0.707 (Butterworth, transparent).
- Existing `ParamRange` mappings for tone knobs are unchanged — only the filter implementation improves.

**CPU impact**: Biquad costs ~4 multiplies + 4 adds per sample vs. ~2+2 for 1st-order. Negligible on H750.

### 2b. Rotary: Stronger AM Depth + Motor Acceleration Curve

**Current state**: Horn AM depth too subtle (~0.3 amplitude swing); motor inertia smoothing exists but time constants not tuned; no acceleration chirp on speed switch.  
**Target**: Match Leslie 122 character — pronounced horn sweep, organic motor spin-up/down.

**Changes**:
- Increase horn AM depth to 0.6–0.7 amplitude swing (tune by ear on hardware).
- Drum AM depth: increase to ~0.4 (drum is less directional than horn).
- Replace linear motor inertia smoothing with exponential ramp: `speed_current += (speed_target - speed_current) * (1 - exp(-dt / tau))` where horn τ ≈ 1.2 s, drum τ ≈ 2.5 s for chorale→tremolo, and τ ≈ 0.8 s / 1.8 s for tremolo→chorale (faster engage than disengage, as on real Leslie).
- Horn cabinet resonance SVF cutoff tracks the `tone` knob (currently fixed at 2.5 kHz).

**CPU impact**: Minimal — replaces linear lerp with one expf per block per motor.

### 2c. Phaser: Stereo Width on All Stage Counts + Feedback Soft-Clip

**Current state**: 16-stage phaser collapses to mono. Feedback can self-oscillate harshly.  
**Target**: All stage counts output true stereo; feedback blooms organically at high resonance.

**Changes**:
- Restore dual allpass chains (L and R) for the 16-stage mode with a fixed small initial phase offset (~15°) between L and R LFOs to create width without comb-filter cancellation.
- Add a soft tanh clip on the feedback path before it re-enters the allpass chain:  
  `fb_clipped = tanh(fb * drive) / drive` where `drive` scales with the regen parameter.  
  At regen = 0 this is transparent; at regen = max it limits smoothly instead of screaming.
- Feedback path gets a DC blocker to prevent low-frequency instability at high regen.

**CPU impact**: Dual chain for 16-stage roughly doubles that mode's CPU. Acceptable given headroom; CPU meter will confirm.

---

## Section 3: Delay Stage

### 3a. Anti-Aliasing Pre-Filter on Modulated Delay Reads

**Current state**: LFO-modulated read pointer moves without pre-filtering, causing aliasing when the effective pitch shift exceeds 1 (reading faster than real-time).  
**Target**: A 1-pole LP pre-filter on the input signal with cutoff that tracks modulation rate.

**Changes**:
- Compute effective modulation rate per block: `mod_rate_hz = lfo_rate * mod_depth_samples`.
- Map to pre-filter cutoff: `cutoff = SAMPLE_RATE/2 * (1 - mod_rate_hz / MAX_MOD_RATE)`, clamped to [2 kHz, 20 kHz].
- Apply a 1-pole LP to the delay line input (before the write pointer) with this cutoff.
- Update the coefficient once per block in `Prepare()`.

**CPU impact**: 1 multiply + 1 add per sample per delay line. Negligible.

### 3b. Soft-Knee Feedback Limiting Across All Delay Modes

**Current state**: Feedback clamped at 0.98 — hard ceiling causes either runaway or abrupt amplitude limiting.  
**Target**: Peak follower + soft-knee gain reduction in the feedback path for musical bloom.

**Changes**:
- Add a `FeedbackLimiter` DSP primitive: envelope follower (attack ~1 ms, release ~200 ms) + gain computer with configurable threshold and knee width.
- Threshold: -3 dBFS. Knee width: 6 dB. Above threshold, gain reduces smoothly via the soft-knee formula.
- All delay modes insert `FeedbackLimiter::Process(feedback_signal)` in their feedback path, removing the hard 0.98 clamp.
- The limiter is stateless enough to be a value type (no heap) — embed directly in each delay mode struct.

**CPU impact**: ~6 multiplies per sample per feedback path. Small.

### 3c. Tape Delay Frequency-Dependent Saturation

**Current state**: `TapeDelay` applies the Tape waveshaper uniformly across all frequencies.  
**Target**: HF-boosted input → saturation → HF-cut output (record/reproduce EQ), giving warm, dark repeats that thicken on the high-grit setting.

**Changes**:
- Pre-saturation: 1st-order HF shelving boost tied to `grit` parameter. At grit=0: flat. At grit=1: +6 dB above ~3 kHz (simulates tape record EQ).
- Post-saturation: matching HF shelving cut. At grit=0: flat. At grit=1: -6 dB above ~3 kHz (simulate reproduce EQ + head gap loss).
- The net result at grit=0 is transparent; at grit=1 the saturation receives a pre-emphasized signal, saturating HF first, then the output de-emphasizes — classic NAB/IEC tape curve character.
- Shelf filter coefficients computed once in `Prepare()` from the `grit` param.

**CPU impact**: 2 additional 1-pole filters in the feedback path. Negligible.

---

## Section 4: Reverb Stage

### 4a. Per-Band RT60 in Hall and Room Reverb

**Current state**: Single RT60 decay value shared across all FDN lines and all frequencies.  
**Target**: LF and HF decay computed independently per line, giving brighter attack and darker sustain — the defining characteristic of real rooms.

**Changes**:
- Each FDN line already has a 1-pole LP damping filter. Repurpose its coefficient to derive two decay coefficients: `g_lf` (full decay) and `g_hf` (derived from tone knob).
- `decay` knob → LF RT60 (unchanged mapping).
- `tone` knob → HF RT60 ratio: tone=0 means HF RT60 = 30% of LF (very dark); tone=1 means HF RT60 = 100% of LF (bright, uniform decay). Default tone=0.6 gives HF RT60 ≈ 60% of LF.
- The 1-pole LP in each FDN feedback path already implements this split naturally — the coefficient just needs to be computed from the HF RT60 target rather than a raw knob value.
- The existing output `ToneFilter` in Hall/Room is removed — its role is superseded by the HF RT60 control inside the FDN, which is more musically accurate (frequency-dependent decay vs. static shelving cut).
- Apply to Hall and Room modes. Cloud and Plate are excluded (their character depends on uniform decay).

**CPU impact**: Zero — replaces one coefficient computation with a slightly different one. No extra filters.

### 4b. Diffuser Delay Ratios Tuned to Fibonacci-Inspired Primes

**Current state**: Delays 142, 107, 672, 413 samples — arbitrary hand-tuned values with suboptimal modal spacing.  
**Target**: Mutually co-prime values in roughly Fibonacci ratio to distribute resonances evenly.

**Proposed new values** (to verify against each other for GCD = 1):
- Stage 0: 149 samples (prime)
- Stage 1: 241 samples (~1.618× 149, prime)
- Stage 2: 389 samples (~1.618× 241, prime)
- Stage 3: 631 samples (~1.618× 389, prime)

All four are prime, so GCD of any pair = 1. This is a constant-swap only — zero CPU impact.

### 4c. Plate Reverb Modulation Depth as User Parameter

**Current state**: Modulation depth in Dattorro plate hardcoded at ±13 samples.  
**Target**: Map depth range ±4–±20 samples to an existing plate parameter (the `depth` knob, currently unused in plate mode or doing something secondary).

**Changes**:
- `PlateReverb::Prepare()` reads the `depth` param and computes `mod_samples = 4 + depth * 16` (linear, range 4–20).
- Pass `mod_samples` to the tank's modulated allpass `SetDepth()` call.
- Tight (4 samples) = studio plate character. Wide (20 samples) = lush, shimmery.
- No change to the algorithm itself — only the depth constant becomes a variable.

**CPU impact**: Zero.

---

## Implementation Order

1. `feature/dsp-quality-v2` branch from `main`
2. CPU meter (display + ISR measurement) — flash and verify readout first
3. ToneFilter biquad upgrade — highest return, lowest risk, affects all modes
4. Rotary AM depth + acceleration — modulation priority
5. Phaser stereo + feedback soft-clip — modulation priority
6. Delay anti-aliasing pre-filter
7. Delay soft-knee feedback limiter (`FeedbackLimiter` primitive)
8. Tape delay frequency-dependent saturation
9. Hall/Room per-band RT60
10. Diffuser Fibonacci primes (constant swap)
11. Plate modulation depth parameter

Each item is independently flashable and testable on hardware before the next begins.

---

## Out of Scope

- Pitch shifter rewrite (Shimmer reverb, Detune chorus — secondary modes)
- Routing matrix / parallel processing paths
- MIDI-tempo-locked LFO cross-stage sync
- New effect modes
