#pragma once
#include "delay_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/svf.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

class FilterDelay : public DelayMode {
public:
    void Init()  override;
    void Reset() override;
    void Prepare(const delay_fx::ParamSet& params) override;
    StereoFrame Process(float input, const delay_fx::ParamSet& params) override;
    const char* Name() const override { return "Filter"; }

private:
    Lfo       lfo_;
    Svf       svf_l_;
    Svf       svf_r_;
    DcBlocker dc_l_;
    DcBlocker dc_r_;

};

} // namespace pedal
