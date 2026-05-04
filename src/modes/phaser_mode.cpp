#include "phaser_mode.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

// Stage counts per sub-mode. Barber Pole (6) uses two 4-stage chains — value unused in that path.
static const int kStageCounts[] = {2, 4, 6, 8, 12, 16, 4};

void PhaserMode::Init() {
    Reset();
}

void PhaserMode::Reset() {
    static constexpr float kHalfPi = 1.57079633f;
    lfo_.Init(0.5f, LfoWave::Sine);
    lfo2_.Init(0.5f, LfoWave::Sine);
    lfo2_.SetPhaseOffset(kHalfPi);   // 90° quadrature offset for Barber Pole
    lfo2_.Reset();                   // apply offset to phase_ (SetPhaseOffset alone does not)
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    dc2_.Init();
    center_    = -0.5f;
    depth_mod_ = 0.0f;
    feedback_  = 0.0f;
    feedback2_ = 0.0f;
    num_stages_  = 4;
    barber_pole_ = false;
}

void PhaserMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    lfo2_.SetRate(params.speed);

    // Sub-mode from p2: 0..6 → stage counts (6 = Barber Pole)
    int sub = static_cast<int>(params.p2 * 6.999f);
    if (sub < 0) sub = 0;
    if (sub > 6) sub = 6;
    barber_pole_ = (sub == 6);
    num_stages_  = kStageCounts[sub];

    // Cache center frequency and depth swing for per-sample LFO use in Process().
    // center: tone=0 → -0.95 (notch ~300 Hz), tone=1 → -0.10 (notch ~10 kHz)
    center_    = -(0.95f - params.tone * 0.85f);
    depth_mod_ = params.depth * 0.4f;
    // LFO coefficients computed per-sample in Process() to avoid block-boundary zipper noise.
}

StereoFrame PhaserMode::Process(StereoFrame input, const ParamSet& params) {
    const float regen = params.p1 * 0.95f;

    // Per-sample LFO values for smooth allpass sweep.
    const float lfo_val = lfo_.Process();
    float coeff = center_ + depth_mod_ * lfo_val;
    if (coeff > -0.01f) coeff = -0.01f;
    if (coeff < -0.99f) coeff = -0.99f;

    if (barber_pole_) {
        // Two independent 4-stage chains with quadrature LFOs.
        // Crossfade based on lfo_val so one chain's sweep hands off to the other,
        // creating the infinite rising/falling illusion.
        const float lfo_val2 = lfo2_.Process();
        float coeff2 = center_ + depth_mod_ * lfo_val2;
        if (coeff2 > -0.01f) coeff2 = -0.01f;
        if (coeff2 < -0.99f) coeff2 = -0.99f;

        float xa = input.mono() + feedback_  * regen;
        float xb = input.mono() + feedback2_ * regen;
        for (int i = 0; i < 4; ++i) {
            stages_[i].SetCoeff(coeff);
            xa = stages_[i].Process(xa);
        }
        for (int i = 4; i < 8; ++i) {
            stages_[i].SetCoeff(coeff2);
            xb = stages_[i].Process(xb);
        }
        // DC-block each chain before feedback and blending.
        // Allpass has unity DC gain; without blocking, residual DC recirculates
        // with gain ≈ regen. Blending two DC-free signals gives DC-free output.
        xa = dc_.Process(xa);
        xb = dc2_.Process(xb);
        feedback_  = xa;
        feedback2_ = xb;
        // lfo_val ∈ [-1,+1]; t=0 → all chain B, t=1 → all chain A
        const float t = (lfo_val + 1.0f) * 0.5f;
        const float out = xa * t + xb * (1.0f - t);
        return {out, out};
    }

    // Normal path: single chain with num_stages_ stages
    float x = input.mono() + feedback_ * regen;
    for (int i = 0; i < num_stages_; ++i) {
        stages_[i].SetCoeff(coeff);
        x = stages_[i].Process(x);
    }
    x = dc_.Process(x);
    feedback_ = x;
    return {x, x};
}

} // namespace pedal
