#include "chorus_mode.h"
#include "../config/constants.h"
#include <cmath>

using namespace pedal::mod_fx;

namespace pedal {

// Chorus needs a longer delay than MAX_MOD_DELAY_SAMPLES (25ms).
// Use 50ms = 2400 samples; SDRAM buffer cost is minimal.
static constexpr size_t kChorusBufSize = 2400;
static float DSY_SDRAM_BSS s_chorus_buf[kChorusBufSize];
static DelayLineSdram s_chorus_line;

static constexpr float PI_2_3 = 2.094395f; // 2π/3 = 120°

void ChorusMode::Init() {
    s_chorus_line.Init(s_chorus_buf, kChorusBufSize);
    for (int i = 0; i < 3; ++i) {
        lfo_[i].Init(0.5f, LfoWave::Sine);
        lfo_[i].SetPhaseOffset(static_cast<float>(i) * PI_2_3);
        lfo_[i].Reset();  // apply offset to phase_ (SetPhaseOffset alone does not)
    }
    dc_.Init();
    dc_r_.Init();
}

void ChorusMode::Reset() {
    s_chorus_line.Reset();
    for (auto& l : lfo_) l.Reset();
    dc_.Init();
    dc_r_.Init();
    bbd_.Reset();
    rand_ = 12345;
    sub_mode_   = 4;
    base_samps_ = 48.0f;
    mod_depth_  = 0.0f;
    delays_[0]  = 0.0f;
    delays_[1]  = 0.0f;
    delays_[2]  = 0.0f;
}

void ChorusMode::Prepare(const ParamSet& params) {
    // Sub-mode: 0=dBucket, 1=Multi, 2=Vibrato, 3=Detune, 4=Digital
    sub_mode_ = static_cast<int>(params.p2 * 4.999f);

    for (auto& l : lfo_) l.SetRate(params.speed);

    // Base delay: p1 maps 1ms..20ms → 48..960 samples
    base_samps_ = 48.0f + params.p1 * 912.0f;

    // LFO depth: params.depth * 480 samples = ±10ms variation.
    // Cap to base_samps-1 so the LFO never swings the delay below 1 sample.
    mod_depth_ = fminf(params.depth * 480.0f, base_samps_ - 1.0f);

    if (sub_mode_ == 3) {
        // Detune: two static offsets, no LFO — compute once here.
        delays_[0] = base_samps_ - mod_depth_ * 0.3f;
        delays_[1] = base_samps_ + mod_depth_ * 0.3f;
        delays_[2] = base_samps_;
        if (delays_[0] < 1.0f) delays_[0] = 1.0f;
        if (delays_[1] >= static_cast<float>(kChorusBufSize - 2))
            delays_[1] = static_cast<float>(kChorusBufSize - 2);
    }
    // Multi and single-voice: LFO advanced per-sample in Process() to avoid
    // block-boundary delay jumps that cause zipper noise at high LFO rates.
}

StereoFrame ChorusMode::Process(StereoFrame input, const ParamSet& params) {
    float write_in = input.mono();

    // dBucket pre-coloration
    if (sub_mode_ == 0) {
        write_in = bbd_.Process(write_in, 0.15f, rand_);
    }

    s_chorus_line.Write(write_in);

    float wet_l, wet_r;

    if (sub_mode_ == 1) {
        // Multi: per-sample LFO for all 3 taps (no block-boundary jumps)
        for (int i = 0; i < 3; ++i) {
            float d = base_samps_ + mod_depth_ * lfo_[i].Process();
            if (d < 1.0f) d = 1.0f;
            if (d >= static_cast<float>(kChorusBufSize - 2))
                d = static_cast<float>(kChorusBufSize - 2);
            delays_[i] = d;
        }
        const float t0 = s_chorus_line.ReadAt(delays_[0]);
        const float t1 = s_chorus_line.ReadAt(delays_[1]);
        const float t2 = s_chorus_line.ReadAt(delays_[2]);
        wet_l = (t0 + t1) * 0.5f;
        wet_r = (t0 + t2) * 0.5f;
    } else if (sub_mode_ == 3) {
        // Detune: L=tap0, R=tap1 (pre-computed in Prepare, no LFO)
        wet_l = s_chorus_line.ReadAt(delays_[0]);
        wet_r = s_chorus_line.ReadAt(delays_[1]);
    } else {
        // Single-voice (dBucket, Vibrato, Digital): per-sample LFO
        float d = base_samps_ + mod_depth_ * lfo_[0].Process();
        if (d < 1.0f) d = 1.0f;
        if (d >= static_cast<float>(kChorusBufSize - 2))
            d = static_cast<float>(kChorusBufSize - 2);
        float wet = s_chorus_line.ReadAt(d);
        if (sub_mode_ == 0) {
            wet = bbd_.Deemphasis(wet);  // dBucket post-coloration
        }
        wet_l = wet_r = wet;
    }

    wet_l = dc_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return {wet_l, wet_r};
}

} // namespace pedal
