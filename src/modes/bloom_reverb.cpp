#include "bloom_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_pre_delay[24000];
static float DSY_SDRAM_BSS buf_diff0[Diffuser::kDelays[0] + 1];
static float DSY_SDRAM_BSS buf_diff1[Diffuser::kDelays[1] + 1];
static float DSY_SDRAM_BSS buf_diff2[Diffuser::kDelays[2] + 1];
static float DSY_SDRAM_BSS buf_diff3[Diffuser::kDelays[3] + 1];
static float DSY_SDRAM_BSS buf_fdn0[2904];
static float DSY_SDRAM_BSS buf_fdn1[3492];
static float DSY_SDRAM_BSS buf_fdn2[4160];
static float DSY_SDRAM_BSS buf_fdn3[4814];

} // namespace

void BloomReverb::Init() {
    pre_delay_.Init(buf_pre_delay, 24000);
    pre_delay_.SetDelay(1.0f);

    float* diff_bufs[Diffuser::STAGES] = {
        buf_diff0, buf_diff1, buf_diff2, buf_diff3
    };
    const size_t diff_sizes[Diffuser::STAGES] = { Diffuser::kDelays[0] + 1, Diffuser::kDelays[1] + 1, Diffuser::kDelays[2] + 1, Diffuser::kDelays[3] + 1 };
    diffuser_.Init(diff_bufs, diff_sizes);
    diffuser_.SetDiffusion(0.65f);

    Fdn::Config fdn_cfg;
    fdn_cfg.n_lines     = 4;
    fdn_cfg.sample_rate = SAMPLE_RATE;
    fdn_cfg.bufs[0]     = buf_fdn0;
    fdn_cfg.bufs[1]     = buf_fdn1;
    fdn_cfg.bufs[2]     = buf_fdn2;
    fdn_cfg.bufs[3]     = buf_fdn3;
    fdn_cfg.delays[0]   = 2903;
    fdn_cfg.delays[1]   = 3491;
    fdn_cfg.delays[2]   = 4159;
    fdn_cfg.delays[3]   = 4813;
    for (int i = 4; i < Fdn::MAX_LINES; ++i) {
        fdn_cfg.bufs[i]   = nullptr;
        fdn_cfg.delays[i] = 0;
    }
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(3.0f);
    fdn_.SetDamping(0.25f);

    tone_.Init();
    bloom_env_       = 0.0f;
    bloom_rate_      = 1.0f / (2.0f * SAMPLE_RATE);
    bloom_feedback_  = 0.0f;
    bloom_fb_signal_ = 0.0f;
}

void BloomReverb::Reset() {
    pre_delay_.Reset();
    diffuser_.Reset();
    fdn_.Reset();
    tone_.Init();
    bloom_env_       = 0.0f;
    bloom_fb_signal_ = 0.0f;
}

void BloomReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    fdn_.SetDamping(0.15f + (1.0f - params.tone) * 0.35f);
    tone_.SetKnob(params.tone);

    const float bloom_time_s = 0.5f + params.param1 * 4.5f;
    bloom_rate_    = 1.0f / (bloom_time_s * SAMPLE_RATE);
    bloom_feedback_ = params.param2 * 0.7f;
}

StereoFrame BloomReverb::Process(float input, const ParamSet& /*params*/) {
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    const float diffused = diffuser_.Process(pre);

    // Bloom envelope rises slowly from 0 toward 1
    bloom_env_ += bloom_rate_ * (1.0f - bloom_env_);

    // FDN input: diffused signal + bloom-gated feedback from previous output
    const float fdn_in = diffused + bloom_feedback_ * bloom_fb_signal_;
    const StereoFrame late = fdn_.Process(fdn_in);

    // Store mono output scaled by bloom envelope for next sample's feedback
    bloom_fb_signal_ = bloom_env_ * 0.5f * (late.left + late.right);

    const StereoFrame out{
        tone_.Process(late.left  * bloom_env_),
        tone_.Process(late.right * bloom_env_)
    };
    return out;
}

} // namespace pedal
