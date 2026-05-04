#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/allpass_filter.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Classic phaser — 2/4/6/8/12/16-stage allpass chain with LFO + regen.
class PhaserMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "Phaser"; }

private:
    static constexpr int kMaxStages = 16;
    Lfo           lfo_;
    Lfo           lfo2_;                  // quadrature LFO for Barber Pole (π/2 offset)
    AllpassFilter stages_[kMaxStages];    // stages[0..3]=chainA, [4..7]=chainB (Barber Pole)
    DcBlocker     dc_;
    DcBlocker     dc2_;  // separate DC blocker for Barber Pole chain B feedback
    float         center_     = -0.5f;   // allpass center coefficient (from tone)
    float         depth_mod_  = 0.0f;   // LFO swing scale (from depth)
    float         feedback_   = 0.0f;   // regen state for chain A / normal
    float         feedback2_  = 0.0f;   // regen state for chain B (Barber Pole)
    int           num_stages_  = 4;      // active stage count (normal modes)
    bool          barber_pole_ = false;  // true when sub-mode 6 is active
};

} // namespace pedal
