#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/allpass_filter.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// UniVibe emulation: 4 allpass stages + amplitude modulation.
/// Warmer than Phaser due to nonlinear LDR response curve + AM component.
class VibeMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "Vibe"; }

private:
    static constexpr int kStages = 4;
    Lfo          lfo_;
    AllpassFilter stages_[kStages];
    DcBlocker    dc_;
    float        feedback_  = 0.0f;  // regen state
    // lfo_coeff and am_gain computed per-sample in Process()
};

} // namespace pedal
