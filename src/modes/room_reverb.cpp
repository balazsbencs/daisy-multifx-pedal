#include "room_reverb.h"
#include "daisy_seed.h"
#include "../config/constants.h"

using namespace pedal::reverb_fx;

namespace pedal {

namespace {

// SDRAM buffers
static float DSY_SDRAM_BSS buf_pre_delay[24000];
static float DSY_SDRAM_BSS buf_er[4096];
static float DSY_SDRAM_BSS buf_diff0[Diffuser::kDelays[0] + 1];
static float DSY_SDRAM_BSS buf_diff1[Diffuser::kDelays[1] + 1];
static float DSY_SDRAM_BSS buf_diff2[Diffuser::kDelays[2] + 1];
static float DSY_SDRAM_BSS buf_diff3[Diffuser::kDelays[3] + 1];
static float DSY_SDRAM_BSS buf_fdn0[1907];
static float DSY_SDRAM_BSS buf_fdn1[2239];
static float DSY_SDRAM_BSS buf_fdn2[2593];
static float DSY_SDRAM_BSS buf_fdn3[2903];
static float DSY_SDRAM_BSS buf_fdn4[3307];
static float DSY_SDRAM_BSS buf_fdn5[3697];
static float DSY_SDRAM_BSS buf_fdn6[4159];
static float DSY_SDRAM_BSS buf_fdn7[4799];

// ER tap table: 8 taps, typical small-room reflections
static constexpr ErTap kErTaps[] = {
    {336,  0.80f, -0.70f},
    {624,  0.70f,  0.70f},
    {912,  0.60f, -0.50f},
    {1392, 0.50f,  0.50f},
    {1776, 0.40f, -0.85f},
    {2256, 0.35f,  0.85f},
    {2832, 0.28f, -0.30f},
    {3504, 0.22f,  0.30f},
};

} // namespace

void RoomReverb::Init() {
    pre_delay_.Init(buf_pre_delay, 24000);
    pre_delay_.SetDelay(1.0f);

    er_.Init(buf_er, 4096);
    er_.SetTaps(kErTaps, 8);

    float* diff_bufs[Diffuser::STAGES] = {
        buf_diff0, buf_diff1, buf_diff2, buf_diff3
    };
    const size_t diff_sizes[Diffuser::STAGES] = { Diffuser::kDelays[0] + 1, Diffuser::kDelays[1] + 1, Diffuser::kDelays[2] + 1, Diffuser::kDelays[3] + 1 };
    diffuser_.Init(diff_bufs, diff_sizes);
    diffuser_.SetDiffusion(0.65f);

    float* fdn_bufs[Fdn::MAX_LINES] = {
        buf_fdn0, buf_fdn1, buf_fdn2, buf_fdn3,
        buf_fdn4, buf_fdn5, buf_fdn6, buf_fdn7
    };
    const size_t fdn_delays[Fdn::MAX_LINES] = {
        1907, 2239, 2593, 2903, 3307, 3697, 4159, 4799
    };
    Fdn::Config fdn_cfg{8, {}, {}, SAMPLE_RATE};
    for (int i = 0; i < 8; ++i) {
        fdn_cfg.bufs[i]   = fdn_bufs[i];
        fdn_cfg.delays[i] = fdn_delays[i];
    }
    fdn_.Init(fdn_cfg);
    fdn_.SetDecay(2.0f);
    fdn_.SetDamping(0.3f);
}

void RoomReverb::Reset() {
    pre_delay_.Reset();
    er_.Reset();
    diffuser_.Reset();
    fdn_.Reset();
}

void RoomReverb::Prepare(const ParamSet& params) {
    const float delay_samples = params.pre_delay * SAMPLE_RATE;
    pre_delay_.SetDelay(delay_samples < 1.0f ? 1.0f : delay_samples);
    fdn_.SetDecay(params.decay);
    fdn_.SetDampFromRt60Ratio(params.decay, 0.30f + params.tone * 0.70f);
    fdn_.SetModulation(params.mod * 8.0f);
    diffuser_.SetDiffusion(params.param2);
}

StereoFrame RoomReverb::Process(float input, const ParamSet& /*params*/) {
    // Pre-delay
    pre_delay_.Write(input);
    const float pre = pre_delay_.Read();

    // Early reflections
    const StereoFrame er = er_.Process(pre);

    // Diffuse and enter FDN
    const float diffused = diffuser_.Process(0.5f * (er.left + er.right));

    // FDN late reverb
    const StereoFrame late = fdn_.Process(diffused);

    // Tone shaping
    const StereoFrame out{
        er.left  * 0.4f + late.left  * 0.6f,
        er.right * 0.4f + late.right * 0.6f
    };
    return out;
}

} // namespace pedal
