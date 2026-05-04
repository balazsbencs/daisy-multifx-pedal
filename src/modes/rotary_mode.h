#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/saturation.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Leslie rotating speaker simulation.
/// Horn (HF) rotates faster than drum (LF); both produce Doppler + AM.
class RotaryMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "Rotary"; }

private:
    Lfo       horn_lfo_;     // fast rotor
    Lfo       horn_lfo_q_;   // 90° offset for stereo spread
    Lfo       drum_lfo_;     // slow rotor
    Lfo       drum_lfo_q_;   // 90° offset
    Saturation drive_;
    DcBlocker  dc_l_, dc_r_;

    float am_depth_    = 0.0f;   // cached params.depth * 0.4
    float horn_mod_    = 0.0f;   // cached depth * 100 samples
    float drum_mod_    = 0.0f;   // cached depth * 200 samples
    float xover_state_ = 0.0f;  // 1-pole LP crossover state
};

} // namespace pedal
