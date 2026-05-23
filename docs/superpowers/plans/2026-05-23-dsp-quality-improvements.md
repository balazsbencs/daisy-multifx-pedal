# DSP Quality Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Raise the sonic ceiling of all three effect stages (mod → delay → reverb) on the Daisy Seed pedal by upgrading filter order, improving modulation character, adding soft feedback limiting, and enabling per-band reverb decay.

**Architecture:** Each task is a self-contained code change that builds and flashes independently. There are no automated tests — verification is: `make -j4` (must succeed), flash, listen. All code is embedded C++17, no heap allocation, 48 kHz / 48-sample blocks on STM32H750 @ 480 MHz.

**Tech Stack:** C++17, libDaisy, DaisySP (not used for DSP here), STM32H750, ST7789 240×320 display, SDRAM for long delay lines.

---

## File Map

| File | Change |
|---|---|
| `src/audio/audio_engine.h` | Add `static volatile float cpu_usage_`; add `static float GetCpuUsage()`; add DWT enable in Init |
| `src/audio/audio_engine.cpp` | Measure DWT cycles in ProcessBlock; write `cpu_usage_` |
| `src/display/display_manager.h` | Add `float cpu_usage_` member; add `SetCpuUsage(float)` |
| `src/display/display_manager.cpp` | Render CPU bar+text in status row; add `SetCpuUsage` |
| `src/main.cpp` | Accumulate CPU readings; call `SetCpuUsage` once per second |
| `src/dsp/tone_filter.h` | Replace 1st-order members with biquad state + coefficients |
| `src/dsp/tone_filter.cpp` | Rewrite SetKnob (biquad coefficients) and Process (DF2T) |
| `src/modes/rotary_mode.h` | No struct changes needed |
| `src/modes/rotary_mode.cpp` | Increase AM depth; tune ramp coefficients; tone-track horn resonance |
| `src/modes/phaser_mode.h` | Add `stages_r_[kMaxStages]`; add `feedback_r_` |
| `src/modes/phaser_mode.cpp` | Use `stages_r_` for R chain in all stage counts; soft-clip feedback |
| `src/modes/digital_delay.h` | Add `float aa_state_l_`, `aa_state_r_` (anti-alias LP state) |
| `src/modes/digital_delay.cpp` | Apply 1-pole LP to write input in Prepare/Process |
| `src/modes/tape_delay.h` | Add `float aa_state_` |
| `src/modes/tape_delay.cpp` | Apply 1-pole LP to write input; replace hard clamp with soft limit |
| `src/dsp/feedback_limiter.h` | New file: `FeedbackLimiter` class (peak follower + soft-knee) |
| `src/modes/digital_delay.cpp` | Embed `FeedbackLimiter`; replace naked feedback sum with limiter output |
| `src/modes/digital_delay.h` | Add `FeedbackLimiter fb_lim_l_`, `fb_lim_r_` |
| `src/modes/tape_delay.h` | Add `FeedbackLimiter fb_lim_` |
| `src/modes/tape_delay.cpp` | Replace hard clamp with `fb_lim_.Process()` |
| `src/modes/tape_delay.h` | Add `float shelf_state_pre_`, `shelf_state_post_`, `shelf_coef_` |
| `src/modes/tape_delay.cpp` | Apply pre/post HF shelf in feedback path |
| `src/dsp/fdn.h` | Add `SetDampFromRt60Ratio(float rt60_lf, float hf_ratio)` |
| `src/dsp/fdn.cpp` | Implement `SetDampFromRt60Ratio` |
| `src/modes/hall_reverb.h` | Remove `ToneFilter tone_` |
| `src/modes/hall_reverb.cpp` | Replace `fdn_.SetDamping(...)` + `tone_` with `fdn_.SetDampFromRt60Ratio(...)`; remove output tone_ call |
| `src/modes/room_reverb.h` | Remove `ToneFilter tone_` |
| `src/modes/room_reverb.cpp` | Same as hall_reverb |
| `src/dsp/diffuser.h` | Change `kDelays[4]` to Fibonacci-prime values |
| `src/modes/plate_reverb.h` | Add `float mod_depth_` member |
| `src/modes/plate_reverb.cpp` | Compute `mod_depth_` from param1 in Prepare; use in Process |

---

## Task 1: CPU Meter — ISR Measurement

**Files:**
- Modify: `src/audio/audio_engine.h`
- Modify: `src/audio/audio_engine.cpp`

- [ ] **Step 1.1: Add cpu_usage_ and GetCpuUsage() to AudioEngine**

In `src/audio/audio_engine.h`, add to the `private:` section and add a static getter:

```cpp
// In public section, after SetHold():
static float GetCpuUsage() { return cpu_usage_; }

// In private section, after last member:
static volatile float cpu_usage_;
```

- [ ] **Step 1.2: Define the static member and instrument ProcessBlock**

In `src/audio/audio_engine.cpp`, add after `AudioEngine* AudioEngine::instance_ = nullptr;`:

```cpp
volatile float AudioEngine::cpu_usage_ = 0.0f;
```

Add DWT enable at the END of `AudioEngine::Init()`, before the closing `}`:

```cpp
// Enable DWT cycle counter for CPU load measurement
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTEN_Msk;
```

Wrap the body of `ProcessBlock` with cycle measurement. Add at the VERY START of `ProcessBlock`, before any existing code:

```cpp
const uint32_t t0 = DWT->CYCCNT;
```

Add at the VERY END of `ProcessBlock`, after `OUT_R[i] = s3.right;` and closing `}` of the for-loop, before the closing `}` of ProcessBlock:

```cpp
const uint32_t t1 = DWT->CYCCNT;
// 480 MHz * 48 samples / 48000 Hz = 480000 cycles per block
static constexpr uint32_t kCyclesPerBlock = 480000u;
const uint32_t elapsed = t1 - t0;
cpu_usage_ = static_cast<float>(elapsed) / static_cast<float>(kCyclesPerBlock);
if (cpu_usage_ > 1.0f) cpu_usage_ = 1.0f;
```

- [ ] **Step 1.3: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build, no errors or warnings about DWT.

- [ ] **Step 1.4: Commit**

```bash
git add src/audio/audio_engine.h src/audio/audio_engine.cpp
git commit -m "feat: CPU load measurement via DWT cycle counter"
```

---

## Task 2: CPU Meter — Display

**Files:**
- Modify: `src/display/display_manager.h`
- Modify: `src/display/display_manager.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 2.1: Add SetCpuUsage to DisplayManager**

In `src/display/display_manager.h`, add to the `public:` section after `Update(...)`:

```cpp
void SetCpuUsage(float usage) { cpu_usage_ = usage; }
```

Add to the `private:` section:

```cpp
float cpu_usage_ = 0.0f;
```

- [ ] **Step 2.2: Render CPU in status row**

In `src/display/display_manager.cpp`, in `Render()`, find the status row section (the block with `if (hold_active)` and `if (preset_event == ...)`). Add this AFTER the preset event rendering block:

```cpp
// CPU usage: always shown, right-aligned before preset event
{
    char cpu_buf[8];
    const int pct = static_cast<int>(cpu_usage_ * 100.0f + 0.5f);
    // Format: "CPU 43%"
    cpu_buf[0] = 'C'; cpu_buf[1] = 'P'; cpu_buf[2] = 'U'; cpu_buf[3] = ' ';
    if (pct >= 100) {
        cpu_buf[4] = '1'; cpu_buf[5] = '0'; cpu_buf[6] = '0'; cpu_buf[7] = '\0';
    } else {
        cpu_buf[4] = static_cast<char>('0' + pct / 10);
        cpu_buf[5] = static_cast<char>('0' + pct % 10);
        cpu_buf[6] = '%'; cpu_buf[7] = '\0';
    }
    const uint16_t cpu_color = (pct >= 80) ? kColorRed : kColorWhite;
    DisplayRenderer::DrawText(90, layout::STATUS_Y, cpu_buf,
                              cpu_color, kColorBlack, Font_6x8);
}
```

- [ ] **Step 2.3: Accumulate and latch CPU reading in main loop**

In `src/main.cpp`, add these static locals just before the `while (true)` loop (after the `display_last_ms = 0` line):

```cpp
static float   cpu_accum_      = 0.0f;
static int     cpu_accum_count = 0;
static float   cpu_display_    = 0.0f;
```

Inside the `while (true)` loop, inside the `if (now - display_last_ms >= DISPLAY_UPDATE_MS)` block, BEFORE the `display.Update(...)` call, add:

```cpp
cpu_accum_ += AudioEngine::GetCpuUsage();
cpu_accum_count++;
if (cpu_accum_count >= 30) {  // update once per second (30 frames × 33ms)
    cpu_display_ = cpu_accum_ / static_cast<float>(cpu_accum_count);
    display.SetCpuUsage(cpu_display_);
    cpu_accum_      = 0.0f;
    cpu_accum_count = 0;
}
```

- [ ] **Step 2.4: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build.

- [ ] **Step 2.5: Flash and verify**

```bash
make program-dfu
```

Expected: "CPU XX%" appears in the middle of the status row on the display. With no FX active, should read roughly 0–5%. With all three FX active in a heavy mode, should read higher.

- [ ] **Step 2.6: Commit**

```bash
git add src/display/display_manager.h src/display/display_manager.cpp src/main.cpp
git commit -m "feat: CPU usage display in status row, updated once per second"
```

---

## Task 3: ToneFilter Biquad Upgrade

**Files:**
- Modify: `src/dsp/tone_filter.h`
- Modify: `src/dsp/tone_filter.cpp`

This is the single highest-return change in the whole plan. Every mode using ToneFilter immediately gets steeper, more musical tone shaping.

- [ ] **Step 3.1: Replace ToneFilter header with biquad version**

Replace the full content of `src/dsp/tone_filter.h`:

```cpp
#pragma once
#include "../config/constants.h"
#include <cmath>

namespace pedal {

// 2nd-order Direct Form II transposed biquad tone filter.
// knob: 0=dark LP, 0.5=flat, 1=bright HP blend
class ToneFilter {
public:
    void Init();
    void SetKnob(float knob);
    float Process(float sample);

private:
    static void ComputeLpCoeffs(float fc, float q,
                                float& b0, float& b1, float& b2,
                                float& a1, float& a2);
    static void ComputeHpCoeffs(float fc, float q,
                                float& b0, float& b1, float& b2,
                                float& a1, float& a2);

    float last_knob_ = -1.0f;

    // LP biquad (DF2T state)
    float lp_b0_ = 1.0f, lp_b1_ = 0.0f, lp_b2_ = 0.0f;
    float lp_a1_ = 0.0f, lp_a2_ = 0.0f;
    float lp_s1_ = 0.0f, lp_s2_ = 0.0f;

    // HP biquad applied to LP output (DF2T state)
    float hp_b0_ = 1.0f, hp_b1_ = 0.0f, hp_b2_ = 0.0f;
    float hp_a1_ = 0.0f, hp_a2_ = 0.0f;
    float hp_s1_ = 0.0f, hp_s2_ = 0.0f;

    float lp_mix_ = 1.0f;
    float hp_mix_ = 0.0f;
};

} // namespace pedal
```

- [ ] **Step 3.2: Rewrite ToneFilter implementation**

Replace the full content of `src/dsp/tone_filter.cpp`:

```cpp
#include "tone_filter.h"
#include <cmath>

namespace pedal {

static constexpr float kTwoPi = 6.28318530717958647692f;

void ToneFilter::ComputeLpCoeffs(float fc, float q,
                                  float& b0, float& b1, float& b2,
                                  float& a1, float& a2) {
    const float w0    = kTwoPi * fc * INV_SAMPLE_RATE;
    const float alpha = sinf(w0) / (2.0f * q);
    const float cw    = cosf(w0);
    const float inv_a0 = 1.0f / (1.0f + alpha);
    b0 = (1.0f - cw) * 0.5f * inv_a0;
    b1 = (1.0f - cw) * inv_a0;
    b2 = b0;
    a1 = -2.0f * cw * inv_a0;
    a2 = (1.0f - alpha) * inv_a0;
}

void ToneFilter::ComputeHpCoeffs(float fc, float q,
                                  float& b0, float& b1, float& b2,
                                  float& a1, float& a2) {
    const float w0    = kTwoPi * fc * INV_SAMPLE_RATE;
    const float alpha = sinf(w0) / (2.0f * q);
    const float cw    = cosf(w0);
    const float inv_a0 = 1.0f / (1.0f + alpha);
    b0 = (1.0f + cw) * 0.5f * inv_a0;
    b1 = -(1.0f + cw) * inv_a0;
    b2 = b0;
    a1 = -2.0f * cw * inv_a0;
    a2 = (1.0f - alpha) * inv_a0;
}

void ToneFilter::Init() {
    lp_s1_ = lp_s2_ = 0.0f;
    hp_s1_ = hp_s2_ = 0.0f;
    SetKnob(0.5f);
}

void ToneFilter::SetKnob(float knob) {
    if (knob == last_knob_) return;
    last_knob_ = knob;

    float lp_fc, hp_fc;
    if (knob <= 0.5f) {
        const float t = knob * 2.0f;
        lp_fc    = 200.0f + t * 7800.0f;  // 200 → 8000 Hz
        hp_fc    = 20.0f;
        lp_mix_  = 1.0f;
        hp_mix_  = 0.0f;
    } else {
        const float t = (knob - 0.5f) * 2.0f;
        lp_fc    = 8000.0f + t * 12000.0f;         // 8000 → 20000 Hz
        hp_fc    = 20.0f + t * t * 2980.0f;        // 20 → 3000 Hz
        lp_mix_  = 1.0f - t;
        hp_mix_  = t;
    }

    ComputeLpCoeffs(lp_fc, 0.707f, lp_b0_, lp_b1_, lp_b2_, lp_a1_, lp_a2_);
    ComputeHpCoeffs(hp_fc, 0.707f, hp_b0_, hp_b1_, hp_b2_, hp_a1_, hp_a2_);
}

float ToneFilter::Process(float x) {
    // LP biquad — Direct Form II transposed
    const float lp_y = lp_b0_ * x + lp_s1_;
    lp_s1_ = lp_b1_ * x - lp_a1_ * lp_y + lp_s2_;
    lp_s2_ = lp_b2_ * x - lp_a2_ * lp_y;

    // HP biquad applied to LP output (series connection)
    const float hp_y = hp_b0_ * lp_y + hp_s1_;
    hp_s1_ = hp_b1_ * lp_y - hp_a1_ * hp_y + hp_s2_;
    hp_s2_ = hp_b2_ * lp_y - hp_a2_ * hp_y;

    return lp_mix_ * lp_y + hp_mix_ * hp_y;
}

} // namespace pedal
```

- [ ] **Step 3.3: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build. ToneFilter is used by DigitalDelay (filter_l_, filter_r_), TapeDelay (filter_), HallReverb (tone_), RoomReverb (tone_), and other delay modes. All compile without changes because the public interface is identical.

- [ ] **Step 3.4: Flash and listen**

```bash
make program-dfu
```

Listen: All delay modes with FILTER knob should have noticeably steeper, more present tone sweep. At knob=0 the low-pass is darker. At knob=1 the high-pass has more bite. No glitches or DC offset.

- [ ] **Step 3.5: Commit**

```bash
git add src/dsp/tone_filter.h src/dsp/tone_filter.cpp
git commit -m "feat: ToneFilter upgraded to 2nd-order biquad (-12dB/oct, Butterworth Q)"
```

---

## Task 4: Rotary — Stronger AM + Exponential Motor Ramp

**Files:**
- Modify: `src/modes/rotary_mode.cpp`

- [ ] **Step 4.1: Tune AM depth and motor ramp coefficients**

In `src/modes/rotary_mode.cpp`, change the ramp coefficient constants at the top of the file. Find:

```cpp
static constexpr float kHornRampCoef = 6.67e-4f;
static constexpr float kDrumRampCoef = 3.33e-4f;
```

Replace with:

```cpp
// Horn τ ≈ 1.2 s → coef = 1 − exp(−BLOCK_SIZE / (SAMPLE_RATE × 1.2))
static constexpr float kHornRampCoef = 8.33e-4f;
// Drum τ ≈ 2.5 s → coef = 1 − exp(−BLOCK_SIZE / (SAMPLE_RATE × 2.5))
static constexpr float kDrumRampCoef = 4.00e-4f;
```

- [ ] **Step 4.2: Increase AM depth and add tone-tracking horn resonance**

In `RotaryMode::Prepare()`, find:

```cpp
am_depth_ = params.depth * 0.5f;
horn_mod_ = params.depth * 90.0f;
drum_mod_ = params.depth * 180.0f;
```

Replace with:

```cpp
am_depth_ = params.depth * 0.65f;   // Leslie 122: horn sweeps ~65% amplitude
horn_mod_ = params.depth * 90.0f;
drum_mod_ = params.depth * 180.0f;
```

In `RotaryMode::Prepare()`, find:

```cpp
// Tone maps to crossover frequency: 0 → 500 Hz, 1 → 2000 Hz
xover_.SetFreq(500.0f + params.tone * 1500.0f);
```

Replace with (add horn cabinet frequency tracking below the xover line):

```cpp
xover_.SetFreq(500.0f + params.tone * 1500.0f);

// Horn cabinet resonance tracks tone: 0 → 1.8 kHz (warm), 1 → 3.5 kHz (bright)
const float horn_fc = 1800.0f + params.tone * 1700.0f;
horn_color_l_.SetFreq(horn_fc);
horn_color_r_.SetFreq(horn_fc);
```

- [ ] **Step 4.3: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build.

- [ ] **Step 4.4: Flash and listen**

```bash
make program-dfu
```

Listen in Rotary mode: switch between chorale (slow) and tremolo (fast) with P2. The motor should ramp up and down with a more organic feel (not instantaneous). At full depth, the amplitude sweep should be notably more pronounced — aim for the Leslie cabinet "chopping" character. Tone knob should shift the horn brightness.

- [ ] **Step 4.5: Commit**

```bash
git add src/modes/rotary_mode.cpp
git commit -m "feat: rotary stronger AM depth, exponential motor ramp, tone-tracking horn resonance"
```

---

## Task 5: Phaser — Stereo for All Stage Counts + Feedback Soft-Clip

**Files:**
- Modify: `src/modes/phaser_mode.h`
- Modify: `src/modes/phaser_mode.cpp`

- [ ] **Step 5.1: Add stages_r_ and feedback_r_ to PhaserMode**

In `src/modes/phaser_mode.h`, find:

```cpp
AllpassFilter stages_[kMaxStages];    // stages[0..3]=chainA, [4..7]=chainB (Barber Pole)
DcBlocker     dc_;
DcBlocker     dc2_;  // separate DC blocker for Barber Pole chain B feedback
float         center_     = -0.5f;   // allpass center coefficient (from tone)
float         depth_mod_  = 0.0f;   // LFO swing scale (from depth)
float         feedback_   = 0.0f;   // regen state for chain A / normal
float         feedback2_  = 0.0f;   // regen state for chain B (Barber Pole)
```

Replace with:

```cpp
AllpassFilter stages_[kMaxStages];    // L chain (all modes) + chainA (Barber Pole)
AllpassFilter stages_r_[kMaxStages];  // R chain (normal modes)
DcBlocker     dc_;
DcBlocker     dc2_;   // Barber Pole chain B / R chain DC blocker
float         center_     = -0.5f;
float         depth_mod_  = 0.0f;
float         feedback_   = 0.0f;   // L / chainA feedback
float         feedback2_  = 0.0f;   // Barber Pole chainB feedback
float         feedback_r_ = 0.0f;   // R chain feedback (normal modes)
```

- [ ] **Step 5.2: Reset stages_r_ and feedback_r_ in Reset()**

In `src/modes/phaser_mode.cpp`, in `PhaserMode::Reset()`, find:

```cpp
for (auto& s : stages_) s.Reset();
```

Add the line immediately after:

```cpp
for (auto& s : stages_r_) s.Reset();
feedback_r_ = 0.0f;
```

- [ ] **Step 5.3: Rewrite the normal (non-Barber-Pole) processing path**

In `PhaserMode::Process()`, find the normal path block starting with:

```cpp
// Normal path: single chain L + quadrature chain R for sub-modes with ≤ 8 stages.
// For 12/16 stage sub-modes (indices 4, 5), run one chain and output mono.
const bool do_stereo = (num_stages_ <= 8);
```

Replace the entire normal path (everything from that comment to `return {xl, xr};`) with:

```cpp
// Normal path: always stereo. L uses stages_[], R uses stages_r_[].
// Both chains run lfo_ and lfo2_ (90° quadrature) for all stage counts.
const float lfo_val = lfo_.Process();
float coeff_l = center_ + depth_mod_ * lfo_val;
if (coeff_l > -0.01f) coeff_l = -0.01f;
if (coeff_l < -0.99f) coeff_l = -0.99f;

// Soft-clip feedback before injection: limits self-oscillation organically.
// drive = 2.0 gives unity gain for small signals, soft limit around ±0.5.
static constexpr float kFbDrive = 2.0f;
const float fb_l_clipped = tanhf(feedback_ * kFbDrive) / kFbDrive;
float xl = input.mono() + fb_l_clipped * regen;
for (int i = 0; i < num_stages_; ++i) {
    float sc = coeff_l;
    if (i & 1) sc += 0.04f * depth_mod_;
    else        sc -= 0.04f * depth_mod_;
    if (sc > -0.01f) sc = -0.01f;
    if (sc < -0.99f) sc = -0.99f;
    stages_[i].SetCoeff(sc);
    xl = stages_[i].Process(xl);
}
xl = dc_.Process(xl);
feedback_ = xl;

// R chain — quadrature LFO gives stereo width on all stage counts.
const float lfo_val2 = lfo2_.Process();
float coeff_r = center_ + depth_mod_ * lfo_val2;
if (coeff_r > -0.01f) coeff_r = -0.01f;
if (coeff_r < -0.99f) coeff_r = -0.99f;

const float fb_r_clipped = tanhf(feedback_r_ * kFbDrive) / kFbDrive;
float xr = input.mono() + fb_r_clipped * regen;
for (int i = 0; i < num_stages_; ++i) {
    float sc = coeff_r;
    if (i & 1) sc += 0.04f * depth_mod_;
    else        sc -= 0.04f * depth_mod_;
    if (sc > -0.01f) sc = -0.01f;
    if (sc < -0.99f) sc = -0.99f;
    stages_r_[i].SetCoeff(sc);
    xr = stages_r_[i].Process(xr);
}
xr = dc2_.Process(xr);
feedback_r_ = xr;

return {xl, xr};
```

Also add `#include <cmath>` at the top of phaser_mode.cpp if not already present (needed for `tanhf`).

- [ ] **Step 5.4: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build. If tanhf is not available, add `#include <cmath>` to phaser_mode.cpp.

- [ ] **Step 5.5: Flash and listen**

```bash
make program-dfu
```

Listen in Phaser mode, sub-mode 12-stage and 16-stage: should now have audible stereo width (was mono before). At high REGEN (P1), the phaser should bloom and sing rather than screech. All stage counts should work correctly.

- [ ] **Step 5.6: Commit**

```bash
git add src/modes/phaser_mode.h src/modes/phaser_mode.cpp
git commit -m "feat: phaser stereo on all stage counts, tanh soft-clip on feedback"
```

---

## Task 6: Delay Anti-Aliasing Pre-Filter

**Files:**
- Modify: `src/modes/digital_delay.h`
- Modify: `src/modes/digital_delay.cpp`
- Modify: `src/modes/tape_delay.h`
- Modify: `src/modes/tape_delay.cpp`

A 1-pole LP on the delay line write input, with cutoff tracking modulation depth×rate. Prevents aliasing when the LFO sweeps the read pointer faster than real-time.

- [ ] **Step 6.1: Add anti-alias state to DigitalDelay**

In `src/modes/digital_delay.h`, in the `private:` section, add after `dc_r_`:

```cpp
float aa_state_l_ = 0.0f;  // anti-alias LP state for L write
float aa_state_r_ = 0.0f;  // anti-alias LP state for R write
float aa_coef_    = 1.0f;  // LP coefficient (1.0 = bypass)
```

- [ ] **Step 6.2: Compute anti-alias coefficient in Prepare()**

In `src/modes/digital_delay.cpp`, in `DigitalDelay::Prepare()`, add at the END:

```cpp
// Anti-alias LP: cutoff tracks mod depth × rate.
// At zero mod this is transparent (coef=1). At max mod it rolls off ~8 kHz.
const float max_mod_rate_hz = 10.0f * 30.0f;  // max speed × max depth_samples
const float mod_rate_hz = params.mod_spd * params.mod_dep * 30.0f;
const float norm = mod_rate_hz / max_mod_rate_hz;
const float aa_fc = 20000.0f - norm * 12000.0f;  // 20kHz → 8kHz
aa_coef_ = 1.0f - expf(-2.0f * 3.14159265f * aa_fc * INV_SAMPLE_RATE);
```

- [ ] **Step 6.3: Apply anti-alias LP before write in Process()**

In `src/modes/digital_delay.cpp`, in `DigitalDelay::Process()`, find:

```cpp
digital_line_l.Write(input + feedback_l);
digital_line_r.Write(input + feedback_r);
```

Replace with:

```cpp
// Anti-alias LP on write input
aa_state_l_ += aa_coef_ * ((input + feedback_l) - aa_state_l_);
aa_state_r_ += aa_coef_ * ((input + feedback_r) - aa_state_r_);
digital_line_l.Write(aa_state_l_);
digital_line_r.Write(aa_state_r_);
```

- [ ] **Step 6.4: Add anti-alias state to TapeDelay**

In `src/modes/tape_delay.h`, add to the `private:` section:

```cpp
float aa_state_ = 0.0f;
float aa_coef_  = 1.0f;
```

In `src/modes/tape_delay.cpp`, in `TapeDelay::Prepare()`, add at the END:

```cpp
const float flutter = params.mod_dep * 50.0f;
const float mod_rate_hz = params.mod_spd * flutter;
const float norm = mod_rate_hz / (10.0f * 50.0f);
const float aa_fc = 20000.0f - norm * 12000.0f;
aa_coef_ = 1.0f - expf(-2.0f * 3.14159265f * aa_fc * INV_SAMPLE_RATE);
```

In `src/modes/tape_delay.cpp`, in `TapeDelay::Process()`, find:

```cpp
tape_line.Write(write_val);
```

Replace with:

```cpp
aa_state_ += aa_coef_ * (write_val - aa_state_);
tape_line.Write(aa_state_);
```

- [ ] **Step 6.5: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build. `expf` requires `<cmath>` — already included in both files via `#include "../config/constants.h"` which brings in `<cmath>`.

- [ ] **Step 6.6: Flash and listen**

```bash
make program-dfu
```

Listen: Digital Delay with high MOD SPD and MOD DEP — modulated repeats should be smoother without harsh aliasing chirps on transients. At low mod settings the sound should be identical to before.

- [ ] **Step 6.7: Commit**

```bash
git add src/modes/digital_delay.h src/modes/digital_delay.cpp \
        src/modes/tape_delay.h src/modes/tape_delay.cpp
git commit -m "feat: anti-aliasing pre-filter on delay write input, tracks mod depth+rate"
```

---

## Task 7: FeedbackLimiter Primitive + Apply to Delay Modes

**Files:**
- Create: `src/dsp/feedback_limiter.h`
- Modify: `src/modes/digital_delay.h`
- Modify: `src/modes/digital_delay.cpp`
- Modify: `src/modes/tape_delay.h`
- Modify: `src/modes/tape_delay.cpp`

- [ ] **Step 7.1: Create FeedbackLimiter**

Create `src/dsp/feedback_limiter.h`:

```cpp
#pragma once
#include <cmath>

namespace pedal {

// Peak follower + soft-knee gain reduction for delay feedback paths.
// Replaces hard ±1.0 clip with a smooth limiter that blooms musically at
// high feedback settings rather than abruptly clamping.
class FeedbackLimiter {
public:
    void Reset() { env_ = 0.0f; }

    float Process(float x) {
        const float env_in = x > 0.0f ? x : -x;
        // Attack ~1ms, release ~200ms at 48kHz
        const float coef = (env_in > env_) ? kAttack : kRelease;
        env_ += coef * (env_in - env_);

        if (env_ <= kThreshold) return x;

        // Soft-knee: gain reduces smoothly above threshold
        const float over = env_ - kThreshold;
        const float gain = kThreshold / (kThreshold + over * kKnee);
        return x * gain;
    }

private:
    static constexpr float kAttack    = 0.0204f;   // 1 − exp(−1/48)
    static constexpr float kRelease   = 0.000104f; // 1 − exp(−1/9600)
    static constexpr float kThreshold = 0.707f;    // −3 dBFS
    static constexpr float kKnee      = 0.5f;      // softer knee = more bloom
    float env_ = 0.0f;
};

} // namespace pedal
```

- [ ] **Step 7.2: Add FeedbackLimiter to DigitalDelay**

In `src/modes/digital_delay.h`, add include:

```cpp
#include "../dsp/feedback_limiter.h"
```

Add to `private:` section:

```cpp
FeedbackLimiter fb_lim_l_;
FeedbackLimiter fb_lim_r_;
```

- [ ] **Step 7.3: Apply limiter in DigitalDelay::Process()**

In `src/modes/digital_delay.cpp`, find:

```cpp
const float feedback_l = wet_l * params.repeats;
const float feedback_r = wet_r * params.repeats;
```

Replace with:

```cpp
const float feedback_l = fb_lim_l_.Process(wet_l * params.repeats);
const float feedback_r = fb_lim_r_.Process(wet_r * params.repeats);
```

- [ ] **Step 7.4: Reset limiters in DigitalDelay::Reset()**

In `DigitalDelay::Reset()`, add:

```cpp
fb_lim_l_.Reset();
fb_lim_r_.Reset();
```

- [ ] **Step 7.5: Add FeedbackLimiter to TapeDelay**

In `src/modes/tape_delay.h`, add include:

```cpp
#include "../dsp/feedback_limiter.h"
```

Add to `private:` section:

```cpp
FeedbackLimiter fb_lim_;
```

In `src/modes/tape_delay.cpp`, in `TapeDelay::Reset()`, add:

```cpp
fb_lim_.Reset();
```

In `TapeDelay::Process()`, find:

```cpp
const float feedback = fb_mono * params.repeats;
float write_val = input + feedback;
if (write_val >  1.0f) write_val =  1.0f;
if (write_val < -1.0f) write_val = -1.0f;
```

Replace with:

```cpp
const float feedback = fb_lim_.Process(fb_mono * params.repeats);
float write_val = input + feedback;
```

- [ ] **Step 7.6: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build.

- [ ] **Step 7.7: Flash and listen**

```bash
make program-dfu
```

Listen: In Digital Delay and Tape Delay, push REPEATS to maximum. The delay should bloom and sustain rather than screech or hard-clip. Backing off REPEATS slightly from max should give musical self-oscillation that fades gracefully.

- [ ] **Step 7.8: Commit**

```bash
git add src/dsp/feedback_limiter.h \
        src/modes/digital_delay.h src/modes/digital_delay.cpp \
        src/modes/tape_delay.h src/modes/tape_delay.cpp
git commit -m "feat: FeedbackLimiter primitive, applied to DigitalDelay and TapeDelay"
```

---

## Task 8: Tape Delay Frequency-Dependent Saturation

**Files:**
- Modify: `src/modes/tape_delay.h`
- Modify: `src/modes/tape_delay.cpp`

Record-EQ boost before saturation → reproduce-EQ cut after: HF saturates first, then de-emphasizes. Zero-grit = fully transparent; max-grit = warm, dark, vintage tape character.

- [ ] **Step 8.1: Add shelf filter state to TapeDelay**

In `src/modes/tape_delay.h`, add to `private:`:

```cpp
float pre_shelf_state_  = 0.0f;  // HF pre-emphasis state
float post_shelf_state_ = 0.0f;  // HF de-emphasis state
float shelf_coef_       = 0.0f;  // 1-pole HP coefficient (~3kHz)
float shelf_gain_       = 0.0f;  // 0 at grit=0, 1 at grit=1
```

- [ ] **Step 8.2: Compute shelf coefficients in Prepare()**

In `src/modes/tape_delay.cpp`, in `TapeDelay::Prepare()`, add at the END:

```cpp
// HF shelf at ~3 kHz for tape record/reproduce EQ simulation.
// shelf_coef_ is the HP coefficient; shelf_gain_ scales the effect.
static constexpr float kShelfFc = 3000.0f;
shelf_coef_ = 1.0f - expf(-2.0f * 3.14159265f * kShelfFc * INV_SAMPLE_RATE);
shelf_gain_ = params.grit;  // 0 = flat, 1 = full tape EQ
```

- [ ] **Step 8.3: Apply pre/post shelf in the feedback path**

In `src/modes/tape_delay.cpp`, in `TapeDelay::Process()`, find:

```cpp
// Feedback is mono-summed and processed through tape color & dynamic HF limiter
float fb_mono = 0.5f * (wet_l + wet_r);
fb_mono = filter_.Process(fb_mono);
fb_mono = sat_.Process(fb_mono);
```

Replace with:

```cpp
float fb_mono = 0.5f * (wet_l + wet_r);

// Pre-emphasis: boost HF before saturation (tape record EQ).
// HP output = fb_mono - LP(fb_mono), scaled by grit.
pre_shelf_state_ += shelf_coef_ * (fb_mono - pre_shelf_state_);
const float pre_hp = fb_mono - pre_shelf_state_;
fb_mono = fb_mono + shelf_gain_ * pre_hp;  // add HF boost

fb_mono = filter_.Process(fb_mono);
fb_mono = sat_.Process(fb_mono);

// De-emphasis: attenuate HF after saturation (tape reproduce EQ).
post_shelf_state_ += shelf_coef_ * (fb_mono - post_shelf_state_);
const float post_hp = fb_mono - post_shelf_state_;
fb_mono = fb_mono - shelf_gain_ * post_hp;  // remove HF boost
```

- [ ] **Step 8.4: Reset shelf states in Reset()**

In `TapeDelay::Reset()`, add:

```cpp
pre_shelf_state_  = 0.0f;
post_shelf_state_ = 0.0f;
```

- [ ] **Step 8.5: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build.

- [ ] **Step 8.6: Flash and listen**

```bash
make program-dfu
```

Listen in Tape Delay: at GRIT=0 it should sound exactly as before. Sweep GRIT to max — the repeats should progressively darken and thicken (HF saturates first due to pre-emphasis, then rolls off). Fast transients should smear into a warm, vintage tape character.

- [ ] **Step 8.7: Commit**

```bash
git add src/modes/tape_delay.h src/modes/tape_delay.cpp
git commit -m "feat: tape delay frequency-dependent saturation via record/reproduce EQ shelves"
```

---

## Task 9: Hall/Room Per-Band RT60

**Files:**
- Modify: `src/dsp/fdn.h`
- Modify: `src/dsp/fdn.cpp`
- Modify: `src/modes/hall_reverb.h`
- Modify: `src/modes/hall_reverb.cpp`
- Modify: `src/modes/room_reverb.h`
- Modify: `src/modes/room_reverb.cpp`

Replace the static output ToneFilter with frequency-dependent decay inside the FDN. Tone knob now controls HF RT60 ratio instead of a shelving cut on output.

- [ ] **Step 9.1: Add SetDampFromRt60Ratio to FDN**

In `src/dsp/fdn.h`, add a new public method declaration after `SetDamping`:

```cpp
// Compute damping from RT60 targets. hf_ratio: HF RT60 = rt60_lf_s * hf_ratio.
// 0.3 = very dark (HF decays 3× faster), 1.0 = uniform (all frequencies decay equally).
void SetDampFromRt60Ratio(float rt60_lf_s, float hf_ratio);
```

- [ ] **Step 9.2: Implement SetDampFromRt60Ratio in fdn.cpp**

In `src/dsp/fdn.cpp`, add the implementation after `SetDamping()`:

```cpp
void Fdn::SetDampFromRt60Ratio(float rt60_lf_s, float hf_ratio) {
    if (rt60_lf_s <= 0.0f) rt60_lf_s = 0.001f;
    if (hf_ratio  <  0.01f) hf_ratio = 0.01f;
    if (hf_ratio  >  1.0f)  hf_ratio = 1.0f;

    const float rt60_hf_s = rt60_lf_s * hf_ratio;

    // Use median line delay for the approximation. This gives consistent
    // HF/LF character across all 8 lines.
    const int   mid      = n_lines_ / 2;
    const float delay_med = delay_s_[mid];

    const float g_lf = expf(-6.9078f * delay_med / rt60_lf_s);
    const float g_hf = expf(-6.9078f * delay_med / rt60_hf_s);

    // From LP filter Nyquist gain: g_nyquist = damp/(2-damp)
    // Solve for damp given g_hf/g_lf ratio:
    //   damp/(2-damp) = g_hf/g_lf  →  damp = 2*g_hf/(g_lf+g_hf)
    const float g_sum = g_lf + g_hf;
    damp_ = (g_sum > 0.001f) ? (2.0f * g_hf / g_sum) : 0.5f;
    if (damp_ < 0.01f) damp_ = 0.01f;
    if (damp_ > 1.0f)  damp_ = 1.0f;
}
```

- [ ] **Step 9.3: Remove ToneFilter from HallReverb header**

In `src/modes/hall_reverb.h`, remove:

```cpp
#include "../dsp/tone_filter.h"
```

And in the `private:` section, remove:

```cpp
ToneFilter       tone_;
```

- [ ] **Step 9.4: Update HallReverb::Prepare() and Process()**

In `src/modes/hall_reverb.cpp`, in `HallReverb::Init()`, remove:

```cpp
tone_.Init();
```

In `HallReverb::Reset()`, remove:

```cpp
tone_.Init();
```

In `HallReverb::Prepare()`, find:

```cpp
fdn_.SetDecay(params.decay);
fdn_.SetDamping(0.15f + params.tone * 0.35f);
fdn_.SetModulation(params.mod * 8.0f);
// Param1 controls pre-diffusion density (0 = minimal, 1 = maximum)
diffuser_.SetDiffusion(0.35f + params.param1 * 0.45f);
tone_.SetKnob(params.tone);
```

Replace with:

```cpp
fdn_.SetDecay(params.decay);
// tone: 0=dark (HF RT60 = 30% of LF), 1=bright (HF RT60 = LF, uniform decay)
fdn_.SetDampFromRt60Ratio(params.decay, 0.30f + params.tone * 0.70f);
fdn_.SetModulation(params.mod * 8.0f);
diffuser_.SetDiffusion(0.35f + params.param1 * 0.45f);
```

In `HallReverb::Process()`, find:

```cpp
const StereoFrame out{
    tone_.Process(er.left  * 0.35f + late.left  * 0.65f),
    tone_.Process(er.right * 0.35f + late.right * 0.65f)
};
```

Replace with:

```cpp
const StereoFrame out{
    er.left  * 0.35f + late.left  * 0.65f,
    er.right * 0.35f + late.right * 0.65f
};
```

- [ ] **Step 9.5: Apply same changes to RoomReverb**

In `src/modes/room_reverb.h`, remove:

```cpp
#include "../dsp/tone_filter.h"
```

And remove `ToneFilter tone_;` from `private:`.

In `src/modes/room_reverb.cpp`, in `RoomReverb::Init()` and `Reset()`, remove `tone_.Init();`.

In `RoomReverb::Prepare()`, find:

```cpp
fdn_.SetDecay(params.decay);
fdn_.SetDamping(0.15f + params.tone * 0.35f);
fdn_.SetModulation(params.mod * 8.0f);
diffuser_.SetDiffusion(params.param2);
tone_.SetKnob(params.tone);
```

Replace with:

```cpp
fdn_.SetDecay(params.decay);
fdn_.SetDampFromRt60Ratio(params.decay, 0.30f + params.tone * 0.70f);
fdn_.SetModulation(params.mod * 8.0f);
diffuser_.SetDiffusion(params.param2);
```

In `RoomReverb::Process()`, find:

```cpp
const StereoFrame out{
    tone_.Process(er.left  * 0.4f + late.left  * 0.6f),
    tone_.Process(er.right * 0.4f + late.right * 0.6f)
};
```

Replace with:

```cpp
const StereoFrame out{
    er.left  * 0.4f + late.left  * 0.6f,
    er.right * 0.4f + late.right * 0.6f
};
```

- [ ] **Step 9.6: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build. If the compiler warns about unused include, confirm `tone_filter.h` is no longer in hall_reverb.h and room_reverb.h.

- [ ] **Step 9.7: Flash and listen**

```bash
make program-dfu
```

Listen in Hall and Room reverb: the TONE knob now controls frequency-dependent decay rather than a static output shelving cut. At tone=0 the HF should decay roughly 3× faster than LF — giving a warm, dark tail. At tone=1 all frequencies decay equally — brighter and airier. This should feel more natural and three-dimensional than the previous output LP filter.

- [ ] **Step 9.8: Commit**

```bash
git add src/dsp/fdn.h src/dsp/fdn.cpp \
        src/modes/hall_reverb.h src/modes/hall_reverb.cpp \
        src/modes/room_reverb.h src/modes/room_reverb.cpp
git commit -m "feat: Hall/Room reverb per-band RT60 via FDN damping from decay ratio"
```

---

## Task 10: Diffuser Fibonacci Prime Delays

**Files:**
- Modify: `src/dsp/diffuser.h`

Zero CPU impact — the SDRAM buffer sizes in hall_reverb.cpp, room_reverb.cpp, and any other reverb that uses `Diffuser::kDelays[i] + 1` will automatically resize at compile time because they reference the constant.

- [ ] **Step 10.1: Update kDelays to Fibonacci-inspired primes**

In `src/dsp/diffuser.h`, find:

```cpp
static constexpr size_t kDelays[STAGES] = {142, 107, 672, 413};
```

Replace with:

```cpp
// Mutually co-prime, Fibonacci-ratio spacing (each ≈ 1.618× previous).
// All four are prime → GCD of any pair = 1 → even modal distribution.
static constexpr size_t kDelays[STAGES] = {149, 241, 389, 631};
```

- [ ] **Step 10.2: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build. All SDRAM buffers that use `Diffuser::kDelays[i] + 1` automatically pick up the new sizes.

- [ ] **Step 10.3: Flash and listen**

```bash
make program-dfu
```

Listen in Hall and Room reverb at high PARAM1 (diffusion) setting: the reverb tail should feel smoother and less colored. The metallic resonance that sometimes appeared at max diffusion should be reduced. Subtle difference — compare with a sustained chord or guitar note through the reverb.

- [ ] **Step 10.4: Commit**

```bash
git add src/dsp/diffuser.h
git commit -m "feat: diffuser delays tuned to Fibonacci-ratio co-prime values for even modal distribution"
```

---

## Task 11: Plate Reverb Modulation Depth Parameter

**Files:**
- Modify: `src/modes/plate_reverb.h`
- Modify: `src/modes/plate_reverb.cpp`

Map PARAM1 to modulation depth (±4..20 samples). This replaces the previous rate-spread feature (param1 = ±12% rate spread) with a more musically useful control.

- [ ] **Step 11.1: Add mod_depth_ member to PlateReverb**

In `src/modes/plate_reverb.h`, add to `private:` after `in_g_lo_`:

```cpp
float mod_depth_ = 13.0f;  // LFO depth in samples; replaces hardcoded kModDepth
```

- [ ] **Step 11.2: Compute mod_depth_ from param1 in Prepare()**

In `src/modes/plate_reverb.cpp`, in `PlateReverb::Prepare()`, find:

```cpp
// Param1: 0 = same rate (coherent modulation), 1 = ±12% rate spread (richer)
const float spread = params.param1 * 0.12f;
lfo_a_.SetRate(mod_rate * (1.0f - spread));
lfo_b_.SetRate(mod_rate * (1.0f + spread));
```

Replace with:

```cpp
// Param1: 0 = tight studio plate (4 samples), 1 = lush shimmer (20 samples)
mod_depth_ = 4.0f + params.param1 * 16.0f;
lfo_a_.SetRate(mod_rate);
lfo_b_.SetRate(mod_rate);
```

- [ ] **Step 11.3: Use mod_depth_ instead of kModDepth in Process()**

In `src/modes/plate_reverb.cpp`, in `PlateReverb::Process()`, find all occurrences of `kModDepth` and replace with `mod_depth_`. There should be two occurrences (one for each tank allpass).

Search for `kModDepth` in plate_reverb.cpp:

```bash
grep -n kModDepth /Users/bbalazs/daisy/multi-fx/src/modes/plate_reverb.cpp
```

For each occurrence, replace `kModDepth` with `mod_depth_`.

- [ ] **Step 11.4: Update the display label for Plate PARAM1**

Check if there is a param descriptor for Plate in the reverb param map. Run:

```bash
grep -r "Plate\|param1_name\|AlgoParam" /Users/bbalazs/daisy/multi-fx/src/params/ | head -30
```

Find where the Plate reverb param1 label is defined and update it from the spread descriptor (if any) to `"DEPTH"` or `"MOD DPTH"`. If there is no existing override (label falls back to generic "PARAM1"), no change is needed.

- [ ] **Step 11.5: Build**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4
```

Expected: clean build.

- [ ] **Step 11.6: Flash and listen**

```bash
make program-dfu
```

Listen in Plate reverb: PARAM1 knob (labeled "PARAM1" or "MOD DPTH") at 0 gives a tight, studio plate character. At max, the modulation is deep and lush — approaching shimmer territory without pitch-shifting. A sustained note should have noticeable pitch movement at high PARAM1.

- [ ] **Step 11.7: Commit**

```bash
git add src/modes/plate_reverb.h src/modes/plate_reverb.cpp
git commit -m "feat: plate reverb modulation depth exposed via PARAM1 (4-20 samples)"
```

---

## Self-Review Checklist

**Spec coverage:**
- [x] CPU meter — Tasks 1–2
- [x] ToneFilter biquad — Task 3
- [x] Rotary AM + acceleration — Task 4
- [x] Phaser stereo + feedback soft-clip — Task 5
- [x] Delay anti-aliasing — Task 6
- [x] Feedback limiter — Task 7
- [x] Tape delay frequency-dependent saturation — Task 8
- [x] Hall/Room per-band RT60 — Task 9
- [x] Diffuser Fibonacci primes — Task 10
- [x] Plate modulation depth — Task 11

**Corrections vs. spec:**
- Spec said ~187 blocks/second but BLOCK_SIZE=48 → 48000/48 = **1000 blocks/second**. Plan uses 30 display frames (×33ms = ~1s) for the accumulation window instead.
- Spec said "depth knob" for plate reverb, but reverb has no depth param. Plan uses **param1** (replacing the rate-spread feature).

**Type consistency:**
- `FeedbackLimiter::Process(float)` → returns float ✓
- `Fdn::SetDampFromRt60Ratio(float, float)` → void ✓
- `ToneFilter::SetKnob(float)` interface unchanged ✓
- `stages_r_[kMaxStages]` same type as `stages_[]` ✓
