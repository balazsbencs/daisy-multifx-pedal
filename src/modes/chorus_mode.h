#pragma once
#include "mod_mode.h"
#include "../dsp/lfo.h"
#include "../dsp/delay_line_sdram.h"
#include "../dsp/bbd_emulator.h"
#include "../dsp/dc_blocker.h"

namespace pedal {

/// Chorus — 5 sub-modes (dBucket/Multi/Vibrato/Detune/Digital) via p2.
class ChorusMode : public ModMode {
public:
    void Init() override;
    void Reset() override;
    void Prepare(const mod_fx::ParamSet& params) override;
    StereoFrame Process(StereoFrame input, const mod_fx::ParamSet& params) override;
    const char* Name() const override { return "Chorus"; }

private:
    Lfo        lfo_[3];        // 3 LFOs for Multi sub-mode (120° offsets)
    BbdEmulator bbd_;
    DcBlocker   dc_;           // DC blocker for left channel
    DcBlocker   dc_r_;         // DC blocker for right channel (Multi/Detune sub-modes)
    uint32_t    rand_ = 12345;
    float       delays_[3] = {};  // Detune: static offsets set in Prepare(); Multi: per-sample LFO taps written and read in Process()
    int         sub_mode_   = 4;  // cached sub-mode index
    float       base_samps_ = 48.0f;
    float       mod_depth_  = 0.0f;
};

} // namespace pedal
