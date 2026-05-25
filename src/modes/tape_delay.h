#pragma once
#include "delay_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/tone_filter.h"
#include "../dsp/saturation.h"
#include "../dsp/dc_blocker.h"
#include "../dsp/feedback_limiter.h"

namespace pedal {

class TapeDelay : public DelayMode {
public:
    void Init()  override;
    void Reset() override;
    void Prepare(const delay_fx::ParamSet& params) override;
    StereoFrame Process(float input, const delay_fx::ParamSet& params) override;
    const char* Name() const override { return "Tape"; }

private:
    Lfo        lfo_;
    ToneFilter filter_;
    Saturation sat_;
    DcBlocker  dc_l_;
    DcBlocker  dc_r_;
    float      lfo_out_ = 0.0f;
    float      env_state_ = 0.0f;
    float      tape_lp_ = 0.0f;
    float      delay_smooth_ = 0.0f;
    float aa_state_ = 0.0f;
    float aa_coef_  = 1.0f;
    FeedbackLimiter fb_lim_;
};

} // namespace pedal
