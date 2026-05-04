#include "tone_filter.h"

namespace pedal {

void ToneFilter::Init() {
    lp_state_ = 0.0f;
    hp_state_ = 0.0f;
    SetKnob(0.5f);
}

void ToneFilter::SetKnob(float knob) {
    // Float equality is intentional: knob comes from a quantized pot/param
    // value, not from arithmetic, so bit-identical repeats are expected.
    if (knob == last_knob_) return;
    last_knob_ = knob;

    // Lower half (0..0.5): pure LP, cutoff sweeps 200 Hz → 8000 Hz, no HP.
    // Upper half (0.5..1): LP cutoff continues 8000 Hz → 20000 Hz (effectively
    // open), HP fades in from 20 Hz → 3000 Hz. LP cutoff is continuous across
    // the boundary (both branches reach 8000 Hz at t=0 / knob=0.5).
    float lp_cutoff, hp_cutoff;
    if (knob <= 0.5f) {
        float t = knob * 2.0f; // 0..1
        lp_cutoff = 200.0f + t * 7800.0f; // 200..8000 Hz
        hp_cutoff = 20.0f;
        lp_mix_ = 1.0f;
        hp_mix_ = 0.0f;
    } else {
        float t = (knob - 0.5f) * 2.0f;        // 0..1
        lp_cutoff = 8000.0f + t * 12000.0f;    // 8000..20000 Hz (continuous from lower half)
        hp_cutoff = 20.0f + t * t * 2980.0f;   // 20..3000 Hz
        lp_mix_ = 1.0f - t;
        hp_mix_ = t;
    }
    lp_coef_ = 1.0f - expf(-2.0f * 3.14159265f * lp_cutoff * INV_SAMPLE_RATE);
    hp_coef_ = 1.0f - expf(-2.0f * 3.14159265f * hp_cutoff * INV_SAMPLE_RATE);
}

float ToneFilter::Process(float sample) {
    lp_state_ += lp_coef_ * (sample    - lp_state_);
    hp_state_ += hp_coef_ * (lp_state_ - hp_state_);
    float hp_out = lp_state_ - hp_state_;
    return lp_mix_ * lp_state_ + hp_mix_ * hp_out;
}

} // namespace pedal
