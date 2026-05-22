#include "shimmer_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_pre_delay[24000];
static float DSY_SDRAM_BSS buf_diff0[143];
static float DSY_SDRAM_BSS buf_diff1[108];
static float DSY_SDRAM_BSS buf_diff2[673];
static float DSY_SDRAM_BSS buf_diff3[414];
static float DSY_SDRAM_BSS buf_fdn0[2730];
static float DSY_SDRAM_BSS buf_fdn1[3252];
static float DSY_SDRAM_BSS buf_fdn2[3864];
static float DSY_SDRAM_BSS buf_fdn3[4508];
static float DSY_SDRAM_BSS buf_pitch0[8192];
static float DSY_SDRAM_BSS buf_pitch1[8192];

} // namespace

void ShimmerReverb::Init() {
    pre_delay_.Init(buf_pre_delay, 24000);
    pre_delay_.SetDelay(1.0f);

    float* diff_bufs[Diffuser::STAGES] = {
        buf_diff0, buf_diff1, buf_diff2, buf_diff3
    };
    const size_t diff_sizes[Diffuser::STAGES] = { 143, 108, 673, 414 };
    diffuser_.Init(diff_bufs, diff_sizes);
    diffuser_.SetDiffusion(0.65f);

    Fdn::Config fdn_cfg;
    fdn_cfg.n_lines     = 4;
    fdn_cfg.sample_rate = SAMPLE_RATE;
    fdn_cfg.bufs[0]     = buf_fdn0;
    fdn_cfg.bufs[1]     = buf_fdn1;
    fdn_cfg.bufs[2]     = buf_fdn2;
    fdn_cfg.bufs[3]     = buf_fdn3;
    fdn_cfg.delays[0]   = 2729;
    fdn_cfg.delays[1]   = 3251;
    fdn_cfg.delays[2]   = 3863;
    fdn_cfg.delays[3]   = 4507;
    for (int i = 4; i < Fdn::MAX_LINES; ++i) {
        fdn_cfg.bufs[i]   = nullptr;
        fdn_cfg.delays[i] = 0;
    }
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(3.0f);
    fdn_.SetDamping(0.2f);

    pitch_shifter_[0].Init(buf_pitch0, 8192, SAMPLE_RATE);
    pitch_shifter_[1].Init(buf_pitch1, 8192, SAMPLE_RATE);
    pitch_shifter_[0].SetShift(12.0f);
    pitch_shifter_[1].SetShift(7.0f);

    tone_.Init();
    hold_           = false;
    pitch_fb_l_     = 0.0f;
    pitch_fb_r_     = 0.0f;
}

void ShimmerReverb::Reset() {
    pre_delay_.Reset();
    diffuser_.Reset();
    fdn_.Reset();
    pitch_shifter_[0].Reset();
    pitch_shifter_[1].Reset();
    tone_.Init();
    hold_           = false;
    pitch_fb_l_     = 0.0f;
    pitch_fb_r_     = 0.0f;
}

void ShimmerReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    fdn_.SetDamping(0.15f + (1.0f - params.tone) * 0.35f);
    tone_.SetKnob(params.tone);

    // Stagger Left and Right pitch shifts by +/-10 cents (0.10 semitones) for stereo width
    pitch_shifter_[0].SetShift(params.param1 - 0.10f);
    pitch_shifter_[1].SetShift(params.param2 + 0.10f);
}

StereoFrame ShimmerReverb::Process(float input, const ParamSet& params) {
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    const float diffused = diffuser_.Process(pre);

    // Combine dry diffused input with previous shimmer feedback in stereo
    const float shimmer_amount = params.mod;
    StereoFrame fdn_in;
    fdn_in.left  = diffused + shimmer_amount * 0.5f * pitch_fb_l_;
    fdn_in.right = diffused + shimmer_amount * 0.5f * pitch_fb_r_;

    const StereoFrame late = fdn_.Process(fdn_in);

    // Pitch-shift FDN output for the next sample's feedback.
    // Feed Left channel output to shifter 0, and Right channel output to shifter 1.
    pitch_fb_l_ = pitch_shifter_[0].Process(late.left);
    pitch_fb_r_ = pitch_shifter_[1].Process(late.right);

    // Blend pitch-shifted outputs directly into the left and right output channels
    const StereoFrame out{
        tone_.Process(late.left  + shimmer_amount * pitch_fb_l_ * 0.5f),
        tone_.Process(late.right + shimmer_amount * pitch_fb_r_ * 0.5f)
    };
    return out;
}

void ShimmerReverb::SetHold(bool h) {
    hold_ = h;
    fdn_.SetHold(h);
}

} // namespace pedal
