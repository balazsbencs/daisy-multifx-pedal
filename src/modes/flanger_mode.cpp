#include "flanger_mode.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

// Fast hyperbolic tangent approximation for soft clipping on STM32
inline float SoftClipTanh(float x) {
    if (x <= -3.0f) return -1.0f;
    if (x >= 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// 10ms max delay = 480 samples + headroom
static constexpr size_t kFlangerBufSize = 512;

static float DSY_SDRAM_BSS s_flanger_buf_l[kFlangerBufSize];
static float DSY_SDRAM_BSS s_flanger_buf_r[kFlangerBufSize];
static DelayLineSdram s_flanger_line_l;
static DelayLineSdram s_flanger_line_r;

void FlangerMode::Init() {
    s_flanger_line_l.Init(s_flanger_buf_l, kFlangerBufSize);
    s_flanger_line_r.Init(s_flanger_buf_r, kFlangerBufSize);

    lfo_.Init(0.3f, LfoWave::Sine);
    lfo_r_.Init(0.3f, LfoWave::Sine);
    lfo_r_.SetPhaseOffset(1.5707963f);  // π/2 radians = 90° stereo spread
    lfo_r_.Reset();  // apply offset to phase_

    dc_l_.Init();
    dc_r_.Init();

    // Initialize custom low-pass filters with a 0.5 coefficient for dark repeats
    fb_lpf_l_.Init(0.5f);
    fb_lpf_r_.Init(0.5f);
}

void FlangerMode::Reset() {
    s_flanger_line_l.Reset();
    s_flanger_line_r.Reset();
    lfo_.Reset();
    lfo_r_.SetPhaseOffset(1.5707963f);
    lfo_r_.Reset();

    dc_l_.Init();
    dc_r_.Init();

    fb_lpf_l_.Init(0.5f);
    fb_lpf_r_.Init(0.5f);
}

void FlangerMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    lfo_r_.SetRate(params.speed);

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
}

StereoFrame FlangerMode::Process(StereoFrame input, const ParamSet& params) {
    // 1. Advance both LFOs per-sample; right channel leads by π/2 for stereo spread
    const float lfo_l = lfo_.Process();
    const float lfo_r = lfo_r_.Process();

    // 2. Calculate delays and clamp to buffer bounds
    float delay_l = (0.5f + 0.5f * lfo_l) * depth_ * max_depth_ + 1.0f;
    float delay_r = (0.5f + 0.5f * lfo_r) * depth_ * max_depth_ + 1.0f;
    if (delay_l < 1.0f) delay_l = 1.0f;
    if (delay_r < 1.0f) delay_r = 1.0f;
    if (delay_l >= static_cast<float>(kFlangerBufSize - 1)) delay_l = static_cast<float>(kFlangerBufSize - 1);
    if (delay_r >= static_cast<float>(kFlangerBufSize - 1)) delay_r = static_cast<float>(kFlangerBufSize - 1);

    // 3. Read with linear interpolation (ReadAt uses the same interp as Read)
    float wet_l = s_flanger_line_l.ReadAt(delay_l);
    float wet_r = s_flanger_line_r.ReadAt(delay_r);

    // 4. Calculate Feedback with Saturation and Low-Pass Damping
    float regen = params.p1 * 0.95f;

    // Soft clip the feedback to sound "analog" and prevent digital harshness
    float fb_l = SoftClipTanh(wet_l * regen * fb_sign_);
    float fb_r = SoftClipTanh(wet_r * regen * fb_sign_);

    // Apply our new 1-pole Low Pass Filter to the feedback loop
    fb_l = fb_lpf_l_.Process(fb_l);
    fb_r = fb_lpf_r_.Process(fb_r);

    // 5. Write to delay line
    s_flanger_line_l.Write(input.left + fb_l);
    s_flanger_line_r.Write(input.right + fb_r);

    // 6. DC block and return wet-only (audio engine applies mix)
    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return {wet_l, wet_r};
}

} // namespace pedal
