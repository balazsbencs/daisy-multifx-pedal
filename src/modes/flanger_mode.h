#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

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
    Lfo      lfo_;
    DcBlocker dc_;
    float    max_depth_ = 240.0f;  // max delay swing for current sub-mode
    float    depth_     = 0.5f;   // cached params.depth
    float    fb_sign_   = 1.0f;   // +1 or -1 from sub-mode
};

} // namespace pedal
