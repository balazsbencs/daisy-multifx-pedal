#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

// A tiny, self-contained 1-pole low pass filter to simulate analog BBD warmth.
struct SimpleLpf {
    float state = 0.0f;
    float coeff = 0.5f;

    void Init(float c = 0.5f) {
        state = 0.0f;
        coeff = c;
    }

    float Process(float input) {
        state = state + coeff * (input - state);
        return state;
    }
};

/// Flanger — short delay + feedback with LFO modulation.
/// 6 sub-modes via p2: Silver/Grey/Black+/Black-/Zero+/Zero-
class FlangerMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "Flanger"; }

private:
    Lfo       lfo_;
    Lfo       lfo_r_;  // right channel LFO, offset by π/2 for stereo spread

    // Stereo DC Blockers
    DcBlocker dc_l_;
    DcBlocker dc_r_;

    // Feedback Low-Pass Filters for analog warmth using our new struct
    SimpleLpf fb_lpf_l_;
    SimpleLpf fb_lpf_r_;

    float     max_depth_ = 240.0f;  // max delay swing for current sub-mode
    float     depth_     = 0.5f;    // cached params.depth
    float     fb_sign_   = 1.0f;    // +1 or -1 from sub-mode
};

} // namespace pedal
