---
title: DSP Blocks
description: Shared DSP building blocks in src/dsp/ used across all effect algorithms
---

All reusable signal processing code lives in `src/dsp/`. These blocks are used by the mode
implementations in `src/modes/` — the effect algorithms compose from them rather than
duplicating DSP logic.

## Blocks

| File | Description |
|------|-------------|
| `delay_line_sdram.h/.cpp` | SDRAM-backed delay line — up to several seconds at 48 kHz. Used by all delay and reverb modes. Allocated with `DSY_SDRAM_BSS` (file-scope static). |
| `fdn.h/.cpp` | Feedback Delay Network — the reverb core used by Hall, Room, Plate, Spring, and Shimmer modes. N×N feedback matrix with per-channel delay lines. |
| `lfo.h/.cpp` | Multi-waveform LFO (sine, triangle, square, S&H). Rate in Hz, phase-continuous. Used by all modulation and delay modes. |
| `svf.h` | State Variable Filter — low-pass, high-pass, band-pass, notch outputs from one structure. Used for TONE parameter across all stages. |
| `allpass.h` | Single allpass section (first-order). Building block for phasers, diffusers. |
| `allpass_filter.h` | Multi-section allpass chain. Used by Phaser and Vibe modes. |
| `diffuser.h` | Schroeder diffuser — series of allpass sections with delay. Used in reverb modes for initial diffusion. |
| `early_reflections.h` | Early reflection generator — sparse delay taps with gain coefficients. Used by Room reverb. |
| `comb_filter.h` | IIR comb filter. Used in plate reverb and some delay modes. |
| `pitch_shifter.h/.cpp` | Granular pitch shifter (overlap-add). Used by Shimmer reverb for the pitch-shifted feedback tail. |
| `envelope_follower.h/.cpp` | RMS envelope follower with configurable attack and release. Used by Auto Swell and Duck Delay modes. |
| `saturation.h` | Soft-clipping waveshaper (tanh approximation). GRIT parameter on Tape and LoFi delay. |
| `waveshaper.h` | General-purpose waveshaper with lookup table. |
| `bbd_emulator.h` | Bucket Brigade Device emulator — modulated delay with bandwidth limiting. Used by BBD delay and Chorus modes for vintage character. |
| `hilbert_transform.h` | Hilbert transform pair for quadrature (90° phase shift). Used by Quadrature mode. |
| `formant_filter.h` | Formant filter bank. Used by Formant mode. |
| `tone_filter.h/.cpp` | Combined LP/HP tone control (shelving). Maps the TONE parameter (0=LP, 0.5=flat, 1=HP). |
| `dc_blocker.h` | DC-blocking high-pass (first-order IIR). Applied at output stage. |
| `fast_math.h` | Fast approximations: `fast_tanh`, `fast_sin`, `fast_exp`. Used wherever accuracy/speed trade-off favours speed. |
| `pattern_sequencer.h` | Step sequencer for rhythmic tremolo and pattern-based delay modes. |

## Usage pattern

Blocks are statically allocated inside mode objects. Example (conceptual):

```cpp
class TapeDelay : public DelayMode {
  DelayLineSDRAM<MAX_DELAY_SAMPLES> DSY_SDRAM_BSS delay_;
  LFO lfo_;
  ToneFilter tone_;
  Saturation sat_;
  ...
};
```

`DSY_SDRAM_BSS` places the delay line in external SDRAM via the linker script.
The LFO and filters fit in DTCMRAM with the mode object.

## Adding a new DSP block

1. Create `src/dsp/my_block.h` (and `.cpp` if it has state needing `.cpp` linkage)
2. Keep the interface minimal: `Init(float sample_rate)` + `Process(float in) → float`
3. Add `DSY_SDRAM_BSS` to any large buffer members
4. Include only from mode files — not from `main.cpp` or hardware code
