#pragma once
#include "../config/constants.h"
#include <cmath>

namespace pedal {

class ToneFilter {
public:
    void Init();
    // knob: 0..1. 0=full LP, 0.5=flat, 1=full HP
    void SetKnob(float knob);
    float Process(float sample);

private:
    // One-pole LP and HP combined
    float last_knob_ = -1.0f; // sentinel: forces recompute on first call
    float lp_coef_ = 0.0f;   // LP coefficient
    float hp_coef_ = 0.0f;   // HP coefficient
    float lp_mix_  = 1.0f;  // mix between LP and HP output
    float hp_mix_  = 0.0f;
    float lp_state_ = 0.0f;
    float hp_state_ = 0.0f;
};

} // namespace pedal
