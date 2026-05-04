#include "plate_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"
#include <cmath>
#include <algorithm>

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_ap0[143];
static float DSY_SDRAM_BSS buf_ap1[108];
static float DSY_SDRAM_BSS buf_ap2[380];
static float DSY_SDRAM_BSS buf_ap3[278];
static float DSY_SDRAM_BSS buf_comb0[4800];
static float DSY_SDRAM_BSS buf_comb1[5000];
static float DSY_SDRAM_BSS buf_comb2[5400];
static float DSY_SDRAM_BSS buf_comb3[5802];

} // namespace

void PlateReverb::Init() {
    allpass_[0].Init(buf_ap0, 143);  allpass_[0].SetDelay(142);
    allpass_[1].Init(buf_ap1, 108);  allpass_[1].SetDelay(107);
    allpass_[2].Init(buf_ap2, 380);  allpass_[2].SetDelay(379);
    allpass_[3].Init(buf_ap3, 278);  allpass_[3].SetDelay(277);

    comb_[0].Init(buf_comb0, 4800);  comb_[0].SetDelay(4799);
    comb_[1].Init(buf_comb1, 5000);  comb_[1].SetDelay(4999);
    comb_[2].Init(buf_comb2, 5400);  comb_[2].SetDelay(5399);
    comb_[3].Init(buf_comb3, 5802);  comb_[3].SetDelay(5801);

    for (auto& c : comb_) {
        c.SetFeedback(0.6f);
        c.SetDamping(0.4f);
    }

    tone_.Init();
    hold_ = false;
}

void PlateReverb::Reset() {
    for (auto& a : allpass_) a.Reset();
    for (auto& c : comb_)    c.Reset();
    tone_.Init();
    hold_ = false;
}

void PlateReverb::Prepare(const ParamSet& params) {
    // RT60 formula: g = exp(-6.9078 * delay_s / decay_s)
    // Average comb delay ≈ 5250 samples
    static constexpr float kAvgDelaySec = 5250.0f / SAMPLE_RATE;
    fb_ = hold_ ? 1.0f : std::min(0.97f, std::exp(-6.9078f * kAvgDelaySec / params.decay));
    const float damp = 0.15f + (1.0f - params.tone) * 0.5f;
    for (auto& c : comb_) {
        c.SetFeedback(fb_);
        c.SetDamping(damp);
    }
    tone_.SetKnob(params.tone);
}

StereoFrame PlateReverb::Process(float input, const ParamSet& /*params*/) {
    // 4-stage allpass diffusion
    float s = input;
    for (auto& a : allpass_) s = a.Process(s, 0.75f);

    // 4 parallel feedback comb filters; (1-fb) normalizes resonant peak to unity
    const float norm  = 1.0f - fb_;
    const float c0 = comb_[0].Process(s) * norm;
    const float c1 = comb_[1].Process(s) * norm;
    const float c2 = comb_[2].Process(s) * norm;
    const float c3 = comb_[3].Process(s) * norm;

    const float left  = tone_.Process(0.5f * (c0 + c2));
    const float right = tone_.Process(0.5f * (c1 + c3));
    return StereoFrame{left, right};
}

void PlateReverb::SetHold(bool h) {
    hold_ = h;
    fb_ = h ? 1.0f : fb_;
    for (auto& c : comb_) c.SetFeedback(fb_);
}

} // namespace pedal
