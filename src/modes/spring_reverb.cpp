#include "spring_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"
#include <cmath>

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

// Spring 0 allpass buffers (base delays: 17,23,31,43,59,79 → buf = delay+1)
static float DSY_SDRAM_BSS s0_ap0[18];
static float DSY_SDRAM_BSS s0_ap1[24];
static float DSY_SDRAM_BSS s0_ap2[32];
static float DSY_SDRAM_BSS s0_ap3[44];
static float DSY_SDRAM_BSS s0_ap4[60];
static float DSY_SDRAM_BSS s0_ap5[80];

// Spring 1 allpass buffers (×1.07)
static float DSY_SDRAM_BSS s1_ap0[19];
static float DSY_SDRAM_BSS s1_ap1[26];
static float DSY_SDRAM_BSS s1_ap2[34];
static float DSY_SDRAM_BSS s1_ap3[47];
static float DSY_SDRAM_BSS s1_ap4[64];
static float DSY_SDRAM_BSS s1_ap5[85];

// Spring 2 allpass buffers (×1.13)
static float DSY_SDRAM_BSS s2_ap0[21];
static float DSY_SDRAM_BSS s2_ap1[27];
static float DSY_SDRAM_BSS s2_ap2[36];
static float DSY_SDRAM_BSS s2_ap3[50];
static float DSY_SDRAM_BSS s2_ap4[67];
static float DSY_SDRAM_BSS s2_ap5[90];

// Comb buffers per spring
static float DSY_SDRAM_BSS s0_comb[4001];
static float DSY_SDRAM_BSS s1_comb[4281];
static float DSY_SDRAM_BSS s2_comb[4521];

} // namespace

// Base allpass delays (spring 0)
static constexpr size_t kApDelays0[6] = { 17, 23, 31, 43, 59, 79 };
// g values descending
static constexpr float  kApG[6]       = { 0.70f, 0.65f, 0.60f, 0.55f, 0.50f, 0.45f };
// Spring 1 allpass delays (×1.07, rounded)
static constexpr size_t kApDelays1[6] = { 18, 25, 33, 46, 63, 85 };
// Spring 2 allpass delays (×1.13, rounded)
static constexpr size_t kApDelays2[6] = { 19, 26, 35, 49, 67, 89 };

void SpringReverb::Init() {
    // Spring 0
    float* sp0_bufs[6] = { s0_ap0, s0_ap1, s0_ap2, s0_ap3, s0_ap4, s0_ap5 };
    const size_t sp0_sizes[6] = { 18, 24, 32, 44, 60, 80 };
    for (int s = 0; s < 6; ++s) {
        ap_[0][s].Init(sp0_bufs[s], sp0_sizes[s]);
        ap_[0][s].SetDelay(kApDelays0[s]);
    }
    comb_[0].Init(s0_comb, 4001);
    comb_[0].SetDelay(4000);

    // Spring 1
    float* sp1_bufs[6] = { s1_ap0, s1_ap1, s1_ap2, s1_ap3, s1_ap4, s1_ap5 };
    const size_t sp1_sizes[6] = { 19, 26, 34, 47, 64, 85 };
    for (int s = 0; s < 6; ++s) {
        ap_[1][s].Init(sp1_bufs[s], sp1_sizes[s]);
        ap_[1][s].SetDelay(kApDelays1[s]);
    }
    comb_[1].Init(s1_comb, 4281);
    comb_[1].SetDelay(4280);

    // Spring 2
    float* sp2_bufs[6] = { s2_ap0, s2_ap1, s2_ap2, s2_ap3, s2_ap4, s2_ap5 };
    const size_t sp2_sizes[6] = { 21, 27, 36, 50, 67, 90 };
    for (int s = 0; s < 6; ++s) {
        ap_[2][s].Init(sp2_bufs[s], sp2_sizes[s]);
        ap_[2][s].SetDelay(kApDelays2[s]);
    }
    comb_[2].Init(s2_comb, 4521);
    comb_[2].SetDelay(4520);

    sat_.Init();
    tone_.Init();
    hold_ = false;
    for (auto& fb : comb_fb_) fb = 0.8f;
}

void SpringReverb::Reset() {
    for (int sp = 0; sp < 3; ++sp) {
        for (int s = 0; s < 6; ++s) ap_[sp][s].Reset();
        comb_[sp].Reset();
    }
    tone_.Init();
    hold_ = false;
}

void SpringReverb::Prepare(const ParamSet& params) {
    // Comb feedback from decay: g = exp(-6.9078 * comb_delay_s / decay_s)
    // comb_delay_s[0] = 4000/48000, [1] = 4280/48000, [2] = 4520/48000
    static constexpr float kCombDelayS[3] = {
        4000.0f / 48000.0f,
        4280.0f / 48000.0f,
        4520.0f / 48000.0f,
    };
    const float decay = params.decay < 0.01f ? 0.01f : params.decay;
    for (int sp = 0; sp < 3; ++sp) {
        comb_fb_[sp] = hold_ ? 1.0f : std::exp(-6.9078f * kCombDelayS[sp] / decay);
    }

    const float damp = 0.15f + params.tone * 0.3f;
    for (int sp = 0; sp < 3; ++sp) {
        comb_[sp].SetFeedback(comb_fb_[sp]);
        comb_[sp].SetDamping(damp);
    }

    // Saturation drive from param1 (Dwell)
    // 0→1.0, 0.25→2.0, 0.5→4.0, 0.75→8.0 → desired_drive = 2^(3*param1)
    // SetDrive(x) sets drive_ = 1 + x*15, so x = (desired_drive - 1) / 15
    const float desired_drive = std::exp(params.param1 * 3.0f * 0.693147f); // 2^(3*p1)
    sat_.SetDrive((desired_drive - 1.0f) * (1.0f / 15.0f));

    tone_.SetKnob(params.tone);
}

StereoFrame SpringReverb::Process(float input, const ParamSet& params) {
    // Apply saturation (Dwell)
    const float saturated = sat_.Process(input);

    // Determine active springs from param2
    const int n_springs = (params.param2 < 0.33f) ? 1
                        : (params.param2 < 0.66f) ? 2
                        : 3;

    float out_l = 0.0f;
    float out_r = 0.0f;

    for (int sp = 0; sp < n_springs; ++sp) {
        // Allpass dispersion chain
        float s = saturated;
        for (int st = 0; st < 6; ++st) {
            s = ap_[sp][st].Process(s, kApG[st]);
        }
        // Feedback comb; (1-fb) normalization bounds resonant peak to unity
        const float c = comb_[sp].Process(s) * (1.0f - comb_fb_[sp]);
        // Alternate L/R per spring
        if (sp == 0)      { out_l += c; out_r += c * 0.7f; }
        else if (sp == 1) { out_l += c * 0.6f; out_r += c; }
        else              { out_l += c * 0.8f; out_r += c * 0.8f; }
    }

    const float scale = 1.0f / static_cast<float>(n_springs);
    return StereoFrame{
        tone_.Process(out_l * scale),
        tone_.Process(out_r * scale)
    };
}

void SpringReverb::SetHold(bool h) {
    hold_ = h;
    if (h) {
        for (auto& c : comb_) c.SetFeedback(1.0f);
    } else {
        for (int sp = 0; sp < 3; ++sp) comb_[sp].SetFeedback(comb_fb_[sp]);
    }
}

} // namespace pedal
