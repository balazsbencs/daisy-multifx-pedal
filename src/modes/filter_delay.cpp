#include "filter_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../dsp/fast_math.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static constexpr float kStereoOffsetSamples = 150.0f;

static float DSY_SDRAM_BSS filter_buf_l[MAX_DELAY_SAMPLES];
static float DSY_SDRAM_BSS filter_buf_r[MAX_DELAY_SAMPLES];
static DelayLineSdram       filter_line_l;
static DelayLineSdram       filter_line_r;

void FilterDelay::Init() {
    filter_line_l.Init(filter_buf_l, MAX_DELAY_SAMPLES);
    filter_line_r.Init(filter_buf_r, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    dc_l_.Init();
    dc_r_.Init();
    svf_l_.Reset();
    svf_r_.Reset();
}

void FilterDelay::Reset() {
    filter_line_l.Reset();
    filter_line_r.Reset();
    dc_l_.Init();
    dc_r_.Init();
    svf_l_.Reset();
    svf_r_.Reset();
    delay_smooth_l_ = 0.0f;
    delay_smooth_r_ = 0.0f;
    fb_lim_l_.Reset();
    fb_lim_r_.Reset();
}

void FilterDelay::Prepare(const ParamSet& params) {
    static constexpr float kDelaySlew = 0.05f;  // block-rate: τ ≈ 20 blocks = 960 samples

    lfo_.SetRate(params.mod_spd);

    float target = params.time * SAMPLE_RATE;
    if (target > static_cast<float>(MAX_DELAY_SAMPLES - 1))
        target = static_cast<float>(MAX_DELAY_SAMPLES - 1);

    delay_smooth_l_ += kDelaySlew * (target - delay_smooth_l_);
    float delay_r = delay_smooth_l_ + kStereoOffsetSamples;
    if (delay_r > static_cast<float>(MAX_DELAY_SAMPLES - 1))
        delay_r = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    delay_smooth_r_ = delay_r;

    filter_line_l.SetDelay(delay_smooth_l_);
    filter_line_r.SetDelay(delay_smooth_r_);

    float q = 0.5f + params.filter * 14.5f;
    svf_l_.SetQ(q);
    svf_r_.SetQ(q);
}

StereoFrame FilterDelay::Process(float input, const ParamSet& params) {
    // Advance LFO per-sample
    float lfo_val = lfo_.Process();

    // Out-of-phase cutoff modulation updated per-sample to eliminate zipper noise (Bug 9)
    float cutoff_l = 800.0f + lfo_val * params.mod_dep * 1500.0f;
    float cutoff_r = 800.0f - lfo_val * params.mod_dep * 1500.0f;

    svf_l_.SetFreq(cutoff_l);
    svf_r_.SetFreq(cutoff_r);

    float wet_l = filter_line_l.Read();
    float wet_r = filter_line_r.Read();

    // Process through the TPT Svf
    svf_l_.Process(wet_l);
    svf_r_.Process(wet_r);

    // Select filter mode based on grit: <0.33 = LP, <0.66 = BP, >=0.66 = HP
    if (params.grit < 0.33f) {
        wet_l = svf_l_.lp();
        wet_r = svf_r_.lp();
    } else if (params.grit < 0.66f) {
        wet_l = svf_l_.bp();
        wet_r = svf_r_.bp();
    } else {
        wet_l = svf_l_.hp();
        wet_r = svf_r_.hp();
    }

    const float feedback_l = fb_lim_l_.Process(wet_l * params.repeats);
    const float feedback_r = fb_lim_r_.Process(wet_r * params.repeats);

    filter_line_l.Write(input + feedback_l);
    filter_line_r.Write(input + feedback_r);

    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return StereoFrame{wet_l, wet_r};
}

} // namespace pedal
