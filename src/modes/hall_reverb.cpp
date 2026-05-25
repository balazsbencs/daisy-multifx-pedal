#include "hall_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_pre_delay[24000];
static float DSY_SDRAM_BSS buf_er[6144];
static float DSY_SDRAM_BSS buf_diff0[Diffuser::kDelays[0] + 1];
static float DSY_SDRAM_BSS buf_diff1[Diffuser::kDelays[1] + 1];
static float DSY_SDRAM_BSS buf_diff2[Diffuser::kDelays[2] + 1];
static float DSY_SDRAM_BSS buf_diff3[Diffuser::kDelays[3] + 1];
static float DSY_SDRAM_BSS buf_fdn0[3307];
static float DSY_SDRAM_BSS buf_fdn1[3697];
static float DSY_SDRAM_BSS buf_fdn2[4159];
static float DSY_SDRAM_BSS buf_fdn3[4799];
static float DSY_SDRAM_BSS buf_fdn4[5297];
static float DSY_SDRAM_BSS buf_fdn5[5903];
static float DSY_SDRAM_BSS buf_fdn6[6397];
static float DSY_SDRAM_BSS buf_fdn7[6997];

static constexpr ErTap kErTaps[8] = {
    { 480,  0.78f, -0.70f},
    { 912,  0.68f,  0.70f},
    {1392,  0.58f, -0.50f},
    {2016,  0.48f,  0.50f},
    {2688,  0.38f, -0.90f},
    {3504,  0.32f,  0.90f},
    {4416,  0.25f, -0.40f},
    {5424,  0.18f,  0.40f},
};

} // namespace

void HallReverb::Init() {
    pre_delay_.Init(buf_pre_delay, 24000);
    pre_delay_.SetDelay(1.0f);

    er_.Init(buf_er, 6144);
    er_.SetTaps(kErTaps, 8);

    float* diff_bufs[Diffuser::STAGES] = {
        buf_diff0, buf_diff1, buf_diff2, buf_diff3
    };
    const size_t diff_sizes[Diffuser::STAGES] = { Diffuser::kDelays[0] + 1, Diffuser::kDelays[1] + 1, Diffuser::kDelays[2] + 1, Diffuser::kDelays[3] + 1 };
    diffuser_.Init(diff_bufs, diff_sizes);
    diffuser_.SetDiffusion(0.65f);

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
    fdn_cfg.delays[0]   = 3307;
    fdn_cfg.delays[1]   = 3697;
    fdn_cfg.delays[2]   = 4159;
    fdn_cfg.delays[3]   = 4799;
    fdn_cfg.delays[4]   = 5297;
    fdn_cfg.delays[5]   = 5903;
    fdn_cfg.delays[6]   = 6397;
    fdn_cfg.delays[7]   = 6997;
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(3.0f);
    fdn_.SetDamping(0.25f);
}

void HallReverb::Reset() {
    pre_delay_.Reset();
    er_.Reset();
    diffuser_.Reset();
    fdn_.Reset();
}

void HallReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    // tone: 0=dark (HF RT60 = 30% of LF), 1=bright (HF RT60 = LF, uniform decay)
    fdn_.SetDampFromRt60Ratio(params.decay, 0.30f + params.tone * 0.70f);
    fdn_.SetModulation(params.mod * 8.0f);
    // Param1 controls pre-diffusion density (0 = minimal, 1 = maximum)
    diffuser_.SetDiffusion(0.35f + params.param1 * 0.45f);
}

StereoFrame HallReverb::Process(float input, const ParamSet& /*params*/) {
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    const StereoFrame er = er_.Process(pre);

    // Weighted asymmetric sum preserves spatial impression from ER stage
    // rather than discarding it with a symmetric 50/50 collapse.
    const float diffused = diffuser_.Process(0.65f * er.left + 0.35f * er.right);

    const StereoFrame late = fdn_.Process(diffused);

    const StereoFrame out{
        er.left  * 0.35f + late.left  * 0.65f,
        er.right * 0.35f + late.right * 0.65f
    };
    return out;
}

} // namespace pedal
