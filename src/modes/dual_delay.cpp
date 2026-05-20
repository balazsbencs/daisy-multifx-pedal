#include "dual_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static constexpr float kStereoOffsetSamples = 150.0f;

static float DSY_SDRAM_BSS dual_buf_l[MAX_DELAY_SAMPLES];
static float DSY_SDRAM_BSS dual_buf_r[MAX_DELAY_SAMPLES];
static DelayLineSdram       dual_line_l;
static DelayLineSdram       dual_line_r;

void DualDelay::Init() {
    dual_line_l.Init(dual_buf_l, MAX_DELAY_SAMPLES);
    dual_line_r.Init(dual_buf_r, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    filter_l_.Init();
    filter_r_.Init();
    filter_l_.SetKnob(0.5f);
    filter_r_.SetKnob(0.5f);
    dc_l_.Init();
    dc_r_.Init();
}

void DualDelay::Reset() {
    dual_line_l.Reset();
    dual_line_r.Reset();
    dc_l_.Init();
    dc_r_.Init();
}

void DualDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();
    filter_l_.SetKnob(params.filter);
    filter_r_.SetKnob(params.filter);
}

StereoFrame DualDelay::Process(float input, const ParamSet& params) {
    const float lfo_val = lfo_out_;
    const float base_samps = params.time * SAMPLE_RATE;

    // Out-of-phase modulation with a tight, musical detuning depth (+/-0.5% max)
    // and a 150-sample (~3.1ms) offset on the right channel.
    const float mod_samps = base_samps * params.mod_dep * 0.005f;
    float delay_l = base_samps + lfo_val * mod_samps;
    float delay_r = base_samps + kStereoOffsetSamples - lfo_val * mod_samps;

    if (delay_l < 1.0f) delay_l = 1.0f;
    if (delay_l > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_l = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }

    if (delay_r < 1.0f) delay_r = 1.0f;
    if (delay_r > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_r = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }

    dual_line_l.SetDelay(delay_l);
    dual_line_r.SetDelay(delay_r);

    float wet_l = dual_line_l.Read();
    float wet_r = dual_line_r.Read();

    wet_l = filter_l_.Process(wet_l);
    wet_r = filter_r_.Process(wet_r);

    // Dynamic ping-pong crossfader based on grit (0.0 = parallel, 1.0 = full ping-pong)
    const float fb_l = wet_l * params.repeats;
    const float fb_r = wet_r * params.repeats;
    const float pp = params.grit;

    const float write_l = input + (1.0f - pp) * fb_l + pp * fb_r;
    const float write_r = input + (1.0f - pp) * fb_r + pp * fb_l;

    dual_line_l.Write(write_l);
    dual_line_r.Write(write_r);

    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return StereoFrame{wet_l, wet_r};
}

} // namespace pedal
