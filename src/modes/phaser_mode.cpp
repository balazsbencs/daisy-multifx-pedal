#include "phaser_mode.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

// Stage counts per sub-mode. Barber Pole (6) uses two 4-stage chains — value unused in that path.
static const int kStageCounts[] = {2, 4, 6, 8, 12, 16, 4};

void PhaserMode::Init() {
    Reset();
}

void PhaserMode::Reset() {
    static constexpr float kHalfPi = 1.57079633f;
    lfo_.Init(0.5f, LfoWave::Sine);
    lfo2_.Init(0.5f, LfoWave::Sine);
    lfo2_.SetPhaseOffset(kHalfPi);   // 90° quadrature offset for Barber Pole
    lfo2_.Reset();                   // apply offset to phase_ (SetPhaseOffset alone does not)
    for (auto& s : stages_) s.Reset();
    dc_.Init();
    dc2_.Init();
    center_    = -0.5f;
    depth_mod_ = 0.0f;
    feedback_  = 0.0f;
    feedback2_ = 0.0f;
    num_stages_  = 4;
    barber_pole_ = false;
}

void PhaserMode::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.speed);
    lfo2_.SetRate(params.speed);

    // Sub-mode from p2: 0..6 → stage counts (6 = Barber Pole)
    int sub = static_cast<int>(params.p2 * 6.999f);
    if (sub < 0) sub = 0;
    if (sub > 6) sub = 6;
    barber_pole_ = (sub == 6);
    num_stages_  = kStageCounts[sub];

    // Cache center frequency and depth swing for per-sample LFO use in Process().
    // center: tone=0 → -0.95 (notch ~300 Hz), tone=1 → -0.10 (notch ~10 kHz)
    center_    = -(0.95f - params.tone * 0.85f);
    depth_mod_ = params.depth * 0.4f;
    // LFO coefficients computed per-sample in Process() to avoid block-boundary zipper noise.
}

StereoFrame FlangerMode::Process(StereoFrame input, const ParamSet& params) {
    // 1. Get LFO value and create a quadrature (90 deg offset) version for the right channel
    const float lfo_l = lfo_.Process();
    const float lfo_r = lfo_.GetPhaseOffset(0.25f); // Assuming your LFO class supports this

    // 2. Calculate delays (avoiding zero for now, assuming standard mode)
    float delay_l = (0.5f + 0.5f * lfo_l) * depth_ * max_depth_ + 1.0f;
    float delay_r = (0.5f + 0.5f * lfo_r) * depth_ * max_depth_ + 1.0f;

    s_flanger_line_l.SetDelay(delay_l);
    s_flanger_line_r.SetDelay(delay_r);

    // 3. Read using High-Quality Interpolation (Hermite recommended)
    float wet_l = s_flanger_line_l.ReadHermite();
    float wet_r = s_flanger_line_r.ReadHermite();

    // 4. Calculate Feedback with Saturation and Low-Pass Damping
    float regen = params.p1 * 0.95f;

    // Soft clip the feedback to sound "analog" and prevent digital harshness
    float fb_l = SoftClipTanh(wet_l * regen * fb_sign_);
    float fb_r = SoftClipTanh(wet_r * regen * fb_sign_);

    // Apply 1-pole Low Pass Filter to the feedback loop (darkens repeats)
    fb_l = fb_lpf_l_.Process(fb_l);
    fb_r = fb_lpf_r_.Process(fb_r);

    // 5. Write to delay line
    s_flanger_line_l.Write(input.l + fb_l);
    s_flanger_line_r.Write(input.r + fb_r);

    // 6. DC Block the wet signals
    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    // 7. Mix Dry and Wet for the Flanger Comb Filter effect!
    float mix = 0.5f; // 50/50 mix gives deepest notches
    float out_l = (input.l * (1.0f - mix)) + (wet_l * mix);
    float out_r = (input.r * (1.0f - mix)) + (wet_r * mix);

    return {out_l, out_r};
}

} // namespace pedal
