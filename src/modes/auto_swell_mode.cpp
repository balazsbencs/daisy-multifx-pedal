#include "auto_swell_mode.h"
#include "../config/constants.h"
#include <cmath>

using namespace pedal::mod_fx;

namespace pedal {

// Small delay buffer for optional shimmer/doubling effect (P2)
static constexpr size_t kSwellBufSize   = 1200;
// Fixed shimmer delay: 580 samples ≈ 12 ms — creates comb/doubling, not modulated chorus
static constexpr float  kSwellChorusDelay = 580.0f;
static float DSY_SDRAM_BSS s_swell_buf[kSwellBufSize];
static DelayLineSdram s_swell_line;

void AutoSwellMode::Init() {
    s_swell_line.Init(s_swell_buf, kSwellBufSize);
    env_.Init(5.0f, 200.0f);
    dc_.Init();
    dc_r_.Init();
}

void AutoSwellMode::Reset() {
    s_swell_line.Reset();
    env_.Init(5.0f, 200.0f);
    dc_.Init();
    dc_r_.Init();
    swell_gain_  = 0.0f;
    thresh_env_ = 0.05f;
}

void AutoSwellMode::Prepare(const ParamSet& params) {
    // Speed (0.010..0.500 s): attack time in seconds (short = fast swell)
    const float attack_ms  = params.speed * 1000.0f;           // 10ms..500ms
    const float release_ms = 50.0f + params.p1 * 1950.0f;      // 50ms..2000ms

    // One-pole IIR coefficients: α = 1 - exp(-1 / (τ * fs))
    swell_coef_ = 1.0f - expf(-1.0f / (attack_ms  * 0.001f * SAMPLE_RATE));
    duck_coef_  = 1.0f - expf(-1.0f / (release_ms * 0.001f * SAMPLE_RATE));
}

StereoFrame AutoSwellMode::Process(StereoFrame input, const ParamSet& params) {
    const float mono_in = input.mono();
    const float env_val = env_.Process(mono_in);  // 0..1 signal level

    // Adaptive threshold: τ ≈ 400 ms slow tracker
    static constexpr float kThreshSlew = 1.0f / 19200.0f;
    thresh_env_ += kThreshSlew * (env_val - thresh_env_);
    const float threshold = thresh_env_ * 1.5f > 0.05f ? thresh_env_ * 1.5f : 0.05f;
    if (env_val > threshold) {
        // Input is loud: kill gain quickly (duck)
        swell_gain_ += duck_coef_ * (0.0f - swell_gain_);
    } else {
        // Input is quiet: slowly ramp gain up (the swell opens)
        swell_gain_ += swell_coef_ * (1.0f - swell_gain_);
    }

    // depth boosts the swell peak (+6 dB max); apply to each channel independently.
    const float scale = swell_gain_ * (1.0f + params.depth);
    float wet_l = input.left  * scale;
    float wet_r = input.right * scale;
    if (wet_l >  1.0f) wet_l =  1.0f;
    if (wet_l < -1.0f) wet_l = -1.0f;
    if (wet_r >  1.0f) wet_r =  1.0f;
    if (wet_r < -1.0f) wet_r = -1.0f;

    // Shimmer/doubling delay line stays mono (fed from mono mix of swelled signal).
    const float shimmer_in = (wet_l + wet_r) * 0.5f;
    s_swell_line.Write(shimmer_in);

    // Optional shimmer from P2: blend in a fixed-delay tap (12 ms doubling, not modulated)
    if (params.p2 > 0.05f) {
        s_swell_line.SetDelay(kSwellChorusDelay);
        const float chorus_wet = s_swell_line.Read();
        const float blend = params.p2 * 0.3f;
        wet_l = wet_l * (1.0f - blend) + chorus_wet * blend;
        wet_r = wet_r * (1.0f - blend) + chorus_wet * blend;
    }

    wet_l = dc_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);
    return {wet_l, wet_r};
}

} // namespace pedal
