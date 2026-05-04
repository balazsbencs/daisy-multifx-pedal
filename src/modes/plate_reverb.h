#pragma once
#include "reverb_mode.h"
#include "../dsp/allpass.h"
#include "../dsp/comb_filter.h"
#include "../dsp/tone_filter.h"

namespace pedal {

class PlateReverb : public ReverbMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const reverb_fx::ParamSet& params) override;
    StereoFrame Process(float input, const reverb_fx::ParamSet& params) override;
    const char* Name() const override { return "Plate"; }
    void SetHold(bool h) override;
    bool SupportsHold() const override { return true; }

private:
    DelayAllpassFilter allpass_[4];
    CombFilter         comb_[4];
    ToneFilter         tone_;
    bool               hold_ = false;
    float              fb_   = 0.6f;
};

} // namespace pedal
