#include "flanger_mode.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

// 10ms max delay = 480 samples + headroom
static constexpr size_t kFlangerBufSize = 512;
static float DSY_SDRAM_BSS s_flanger_buf[kFlangerBufSize];
static DelayLineSdram s_flanger_line;

void FlangerMode::Init() {
    s_flanger_line.Init(s_flanger_buf, kFlangerBufSize);
    lfo_.Init(0.3f, LfoWave::Sine);
    dc_.Init();
}

void FlangerMode::Reset() {
    s_flanger_line.Reset();
    lfo_.Reset();
    dc_.Init();
}

void FlangerMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);

    // Sub-mode from p2: 0=Silver, 1=Grey, 2=Black+, 3=Black-, 4=Zero+, 5=Zero-
    const int sub = static_cast<int>(params.p2 * 5.999f);

    // Determine feedback sign and base depth from sub-mode
    switch (sub) {
        case 0: fb_sign_ =  1.0f; break;  // Silver: positive, moderate
        case 1: fb_sign_ = -1.0f; break;  // Grey: negative, hollow
        case 2: fb_sign_ =  1.0f; break;  // Black+: positive, high regen
        case 3: fb_sign_ = -1.0f; break;  // Black-: negative, high regen
        case 4: fb_sign_ =  1.0f; break;  // Zero+: through-zero emulation (positive)
        case 5: fb_sign_ = -1.0f; break;  // Zero-: through-zero emulation (negative)
        default: fb_sign_ = 1.0f; break;
    }

    // Black and Zero sub-modes allow higher depth
    max_depth_ = (sub >= 2) ? 460.0f : 240.0f;
    depth_ = params.depth;
    // Delay computed per-sample in Process() to avoid block-boundary zipper noise.
}

StereoFrame FlangerMode::Process(StereoFrame input, const ParamSet& params) {
    // Compute delay per-sample for smooth LFO modulation.
    const float lfo_val = lfo_.Process();
    float delay = (0.5f + 0.5f * lfo_val) * depth_ * max_depth_ + 1.0f;
    if (delay < 1.0f) delay = 1.0f;
    if (delay >= static_cast<float>(kFlangerBufSize - 1))
        delay = static_cast<float>(kFlangerBufSize - 1);
    s_flanger_line.SetDelay(delay);
    float wet = s_flanger_line.Read();

    // Feedback clamped to prevent self-oscillation
    float regen = params.p1 * 0.9f;
    if (regen > 0.9f) regen = 0.9f;
    s_flanger_line.Write(input.mono() + wet * regen * fb_sign_);

    wet = dc_.Process(wet);
    return {wet, wet};
}

} // namespace pedal
