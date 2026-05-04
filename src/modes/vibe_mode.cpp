#include "vibe_mode.h"
#include "../config/constants.h"
#include <cmath>

using namespace pedal::mod_fx;

namespace pedal {

void VibeMode::Init() {
    Reset();
}

void VibeMode::Reset() {
    lfo_.Init(1.0f, LfoWave::Sine);
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    feedback_ = 0.0f;
}

void VibeMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    // LFO value and derived coefficients computed per-sample in Process()
    // to avoid block-boundary zipper noise.
}

StereoFrame VibeMode::Process(StereoFrame input, const ParamSet& params) {
    // Per-sample LFO + LDR nonlinearity for smooth UniVibe sweep.
    const float raw_lfo = lfo_.Process(); // -1..+1
    // LDR asymmetric curve: compress positive half, expand negative.
    const float ldr = (raw_lfo >= 0.0f) ? (raw_lfo * raw_lfo) : -(raw_lfo * raw_lfo);

    // Allpass center −0.70 → notch ≈ 1.5 kHz; depth sweeps ±0.25 → 800 Hz – 4 kHz.
    const float lfo_coeff = -0.70f + params.depth * 0.25f * ldr;

    // Amplitude modulation: slight gain reduction on positive LFO half (UniVibe throb).
    float am_gain = 1.0f - params.depth * 0.3f * (0.5f + 0.5f * ldr);
    if (am_gain < 0.1f) am_gain = 0.1f;

    // Regen feedback
    const float regen = params.p1 * 0.7f;
    float x = input.mono() + feedback_ * regen;

    // 4 allpass stages with slight per-stage coefficient offsets (non-uniform)
    const float offsets[4] = {0.0f, 0.1f, -0.1f, 0.05f};
    for (int i = 0; i < kStages; ++i) {
        float c = lfo_coeff + offsets[i] * params.depth;
        if (c > -0.01f) c = -0.01f;  // keep negative so notches stay in audio band
        if (c < -0.99f) c = -0.99f;
        stages_[i].SetCoeff(c);
        x = stages_[i].Process(x);
    }

    x *= am_gain;
    x = dc_.Process(x);
    feedback_ = x;
    return {x, x};
}

} // namespace pedal
