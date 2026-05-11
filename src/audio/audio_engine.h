#pragma once
#include "daisy_seed.h"
#include "stereo_frame.h"
#include "../params/delay_param_set.h"
#include "../params/mod_param_set.h"
#include "../params/reverb_param_set.h"
#include "../modes/delay_mode.h"
#include "../modes/mod_mode.h"
#include "../modes/reverb_mode.h"

namespace pedal {

struct MultiParamBuf {
    mod_fx::ParamSet    mod;
    delay_fx::ParamSet  delay;
    reverb_fx::ParamSet reverb;
    bool                hold_active;
    bool                fx_enabled[3] = {false, false, false}; // 0=mod,1=delay,2=reverb — off by default
};

class AudioEngine {
public:
    static AudioEngine* instance_;

    void Init(daisy::DaisySeed* hw);

    // Main-loop API — all use ScopedIrqBlocker internally
    void SetModMode(ModMode* m);
    void SetDelayMode(DelayMode* m);
    void SetReverbMode(ReverbMode* m);
    void SetParams(const MultiParamBuf& params);
    void SetHold(bool hold);

    static void AudioCallback(daisy::AudioHandle::InputBuffer  in,
                              daisy::AudioHandle::OutputBuffer out,
                              size_t                           size);

private:
    void ProcessBlock(daisy::AudioHandle::InputBuffer  in,
                      daisy::AudioHandle::OutputBuffer out,
                      size_t                           size);

    // Constant-power mix coefficients, cached per stage
    void UpdateMixCoeffs(const MultiParamBuf& p);

    daisy::DaisySeed* hw_  = nullptr;
    ModMode*    mod_mode_   = nullptr;
    DelayMode*  delay_mode_ = nullptr;
    ReverbMode* reverb_mode_= nullptr;

    MultiParamBuf param_buf_[2]{};
    volatile int  param_read_idx_ = 0;
    volatile bool param_dirty_    = false;
    volatile bool hold_dirty_     = false;
    volatile bool new_hold_       = false;

    // Per-stage cached mix coefficients
    float last_mod_mix_   = -1.0f;
    float mod_dry_  = 1.0f, mod_wet_  = 0.0f, mod_norm_  = 1.0f;

    float last_dly_mix_   = -1.0f;
    float dly_dry_  = 1.0f, dly_wet_  = 0.0f, dly_norm_  = 1.0f;

    float last_rev_mix_   = -1.0f;
    float rev_dry_  = 1.0f, rev_wet_  = 0.0f, rev_norm_  = 1.0f;
};

} // namespace pedal
