#include "cloud_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_pre_delay[24000];
// Diffuser 0 buffers
static float DSY_SDRAM_BSS buf_d0_0[Diffuser::kDelays[0] + 1];
static float DSY_SDRAM_BSS buf_d0_1[Diffuser::kDelays[1] + 1];
static float DSY_SDRAM_BSS buf_d0_2[Diffuser::kDelays[2] + 1];
static float DSY_SDRAM_BSS buf_d0_3[Diffuser::kDelays[3] + 1];
// Diffuser 1 buffers (scaled up)
static float DSY_SDRAM_BSS buf_d1_0[211];
static float DSY_SDRAM_BSS buf_d1_1[157];
static float DSY_SDRAM_BSS buf_d1_2[Diffuser::kDelays[2] + 1];
static float DSY_SDRAM_BSS buf_d1_3[Diffuser::kDelays[3] + 1];
// FDN 8-line buffers
static float DSY_SDRAM_BSS buf_fdn0[4802];
static float DSY_SDRAM_BSS buf_fdn1[5504];
static float DSY_SDRAM_BSS buf_fdn2[6152];
static float DSY_SDRAM_BSS buf_fdn3[7002];
static float DSY_SDRAM_BSS buf_fdn4[7700];
static float DSY_SDRAM_BSS buf_fdn5[8504];
static float DSY_SDRAM_BSS buf_fdn6[9002];
static float DSY_SDRAM_BSS buf_fdn7[9884];

} // namespace

void CloudReverb::Init() {
    pre_delay_.Init(buf_pre_delay, 24000);
    pre_delay_.SetDelay(1.0f);

    float* d0_bufs[Diffuser::STAGES] = {
        buf_d0_0, buf_d0_1, buf_d0_2, buf_d0_3
    };
    const size_t d0_sizes[Diffuser::STAGES] = { Diffuser::kDelays[0] + 1, Diffuser::kDelays[1] + 1, Diffuser::kDelays[2] + 1, Diffuser::kDelays[3] + 1 };
    diffuser0_.Init(d0_bufs, d0_sizes);
    diffuser0_.SetDiffusion(0.7f);

    float* d1_bufs[Diffuser::STAGES] = {
        buf_d1_0, buf_d1_1, buf_d1_2, buf_d1_3
    };
    const size_t d1_sizes[Diffuser::STAGES] = { 211, 157, Diffuser::kDelays[2] + 1, Diffuser::kDelays[3] + 1 };
    diffuser1_.Init(d1_bufs, d1_sizes);
    diffuser1_.SetDiffusion(0.7f);

    Fdn::Config fdn_cfg;
    fdn_cfg.n_lines     = 8;
    fdn_cfg.sample_rate = SAMPLE_RATE;
    fdn_cfg.bufs[0]     = buf_fdn0;
    fdn_cfg.bufs[1]     = buf_fdn1;
    fdn_cfg.bufs[2]     = buf_fdn2;
    fdn_cfg.bufs[3]     = buf_fdn3;
    fdn_cfg.bufs[4]     = buf_fdn4;
    fdn_cfg.bufs[5]     = buf_fdn5;
    fdn_cfg.bufs[6]     = buf_fdn6;
    fdn_cfg.bufs[7]     = buf_fdn7;
    fdn_cfg.delays[0]   = 4801;
    fdn_cfg.delays[1]   = 5503;
    fdn_cfg.delays[2]   = 6151;
    fdn_cfg.delays[3]   = 7001;
    fdn_cfg.delays[4]   = 7699;
    fdn_cfg.delays[5]   = 8503;
    fdn_cfg.delays[6]   = 9001;
    fdn_cfg.delays[7]   = 9883;
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(10.0f);
    fdn_.SetDamping(0.3f);

    tone_.Init();
}

void CloudReverb::Reset() {
    pre_delay_.Reset();
    diffuser0_.Reset();
    diffuser1_.Reset();
    fdn_.Reset();
    tone_.Init();
}

void CloudReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    fdn_.SetModulation(params.mod * 20.0f);

    const float diff = 0.5f + params.param1 * 0.35f;
    diffuser0_.SetDiffusion(diff);
    diffuser1_.SetDiffusion(diff);

    const float damp = 0.1f + params.param2 * 0.4f;
    fdn_.SetDamping(damp);

    tone_.SetKnob(params.tone);
}

StereoFrame CloudReverb::Process(float input, const ParamSet& /*params*/) {
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    // Two 4-stage diffusers in cascade (= 8-stage total diffusion)
    float diffused = diffuser0_.Process(pre);
    diffused        = diffuser1_.Process(diffused);

    const StereoFrame late = fdn_.Process(diffused);

    const StereoFrame out{
        tone_.Process(late.left),
        tone_.Process(late.right)
    };
    return out;
}

} // namespace pedal
