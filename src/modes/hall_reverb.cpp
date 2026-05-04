#include "hall_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

static float DSY_SDRAM_BSS buf_pre_delay[24000];
static float DSY_SDRAM_BSS buf_er[6144];
static float DSY_SDRAM_BSS buf_diff0[143];
static float DSY_SDRAM_BSS buf_diff1[108];
static float DSY_SDRAM_BSS buf_diff2[380];
static float DSY_SDRAM_BSS buf_diff3[278];
static float DSY_SDRAM_BSS buf_fdn0[3548];
static float DSY_SDRAM_BSS buf_fdn1[4134];
static float DSY_SDRAM_BSS buf_fdn2[4920];
static float DSY_SDRAM_BSS buf_fdn3[5690];

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
    const size_t diff_sizes[Diffuser::STAGES] = { 143, 108, 380, 278 };
    diffuser_.Init(diff_bufs, diff_sizes);
    diffuser_.SetDiffusion(0.65f);

    Fdn::Config fdn_cfg;
    fdn_cfg.n_lines    = 4;
    fdn_cfg.sample_rate = SAMPLE_RATE;
    fdn_cfg.bufs[0]    = buf_fdn0;
    fdn_cfg.bufs[1]    = buf_fdn1;
    fdn_cfg.bufs[2]    = buf_fdn2;
    fdn_cfg.bufs[3]    = buf_fdn3;
    fdn_cfg.delays[0]  = 3547;
    fdn_cfg.delays[1]  = 4133;
    fdn_cfg.delays[2]  = 4919;
    fdn_cfg.delays[3]  = 5689;
    for (int i = 4; i < Fdn::MAX_LINES; ++i) {
        fdn_cfg.bufs[i]   = nullptr;
        fdn_cfg.delays[i] = 0;
    }
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(3.0f);
    fdn_.SetDamping(0.25f);

    tone_.Init();
}

void HallReverb::Reset() {
    pre_delay_.Reset();
    er_.Reset();
    diffuser_.Reset();
    fdn_.Reset();
    tone_.Init();
}

void HallReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    fdn_.SetDamping(0.15f + (1.0f - params.tone) * 0.35f);
    tone_.SetKnob(params.tone);
}

StereoFrame HallReverb::Process(float input, const ParamSet& /*params*/) {
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    const StereoFrame er = er_.Process(pre);

    const float diffused = diffuser_.Process(0.5f * (er.left + er.right));

    const StereoFrame late = fdn_.Process(diffused);

    const StereoFrame out{
        tone_.Process(er.left  * 0.35f + late.left  * 0.65f),
        tone_.Process(er.right * 0.35f + late.right * 0.65f)
    };
    return out;
}

} // namespace pedal
