#include "formant_mode.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

struct FormantFreqs { float f1, f2; };

static const FormantFreqs kVowels[5] = {
    {730.0f, 1090.0f},  // AA
    {270.0f, 2290.0f},  // EE
    {400.0f, 2050.0f},  // EYE
    {570.0f,  840.0f},  // OH
    {300.0f,  870.0f},  // OOH
};

void FormantMode::Init() {
    Reset();
}

void FormantMode::Reset() {
    lfo_.Init(0.5f, LfoWave::Sine);
    f1_.Reset(); f2_.Reset();
    f1_.SetQ(4.0f);
    f2_.SetQ(6.0f);
    dc_.Init();
    f1_hz_       = kVowels[0].f1;
    f2_hz_       = kVowels[0].f2;
    target_f1_   = kVowels[0].f1;
    target_f2_   = kVowels[0].f2;
    morph_depth_ = 0.5f;
    update_count_ = 0;
}

void FormantMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Target vowel from p2 (0..1 -> 5 vowels)
    const int vowel = static_cast<int>(params.p2 * 4.999f);
    target_f1_ = kVowels[vowel].f1;
    target_f2_ = kVowels[vowel].f2;
    morph_depth_ = params.depth;

    // P1 controls formant bandwidth (Q: higher = narrower)
    const float q1 = 2.0f + params.p1 * 8.0f;
    const float q2 = 4.0f + params.p1 * 10.0f;
    f1_.SetQ(q1);
    f2_.SetQ(q2);

    // Apply current formant frequencies (updated per-sample in Process via slew).
    // SetFreq calls tanf() — safe here at block rate, not per-sample.
    f1_.SetFreq(f1_hz_);
    f2_.SetFreq(f2_hz_);
}

StereoFrame FormantMode::Process(StereoFrame input, const ParamSet& /*params*/) {
    // Per-sample LFO for smooth formant morph
    const float lfo_val = lfo_.Process();
    const float t = 0.5f + 0.5f * lfo_val;    // 0..1
    const float morph = morph_depth_ * t;

    // Slew formant frequencies toward target
    f1_hz_ += (target_f1_ - f1_hz_) * morph * 0.1f;
    f2_hz_ += (target_f2_ - f2_hz_) * morph * 0.1f;

    // Update SVF coefficients every 16 samples to amortize tanf() cost
    // while still tracking the per-sample LFO smoothly.
    if (++update_count_ >= 16) {
        update_count_ = 0;
        f1_.SetFreq(f1_hz_);
        f2_.SetFreq(f2_hz_);
    }

    const float mono = input.mono();
    f1_.Process(mono);
    f2_.Process(mono);
    float wet = (f1_.bp() + f2_.bp()) * 0.5f;
    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal
