#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"

namespace pedal {

/// Amplitude tremolo — 3 sub-modes via p2.
class VintageTremMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "VintTrem"; }

private:
    Lfo   lfo_;
    float depth_ = 0.5f;  // cached params.depth for per-sample gain computation
};

} // namespace pedal
