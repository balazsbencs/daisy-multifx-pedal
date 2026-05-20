#include "digital_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS digital_buf_l[MAX_DELAY_SAMPLES];
static float DSY_SDRAM_BSS digital_buf_r[MAX_DELAY_SAMPLES];
static DelayLineSdram       digital_line_l;
static DelayLineSdram       digital_line_r;

void DigitalDelay::Init() {
    digital_line_l.Init(digital_buf_l, MAX_DELAY_SAMPLES);
    digital_line_r.Init(digital_buf_r, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    filter_l_.Init();
    filter_r_.Init();
    filter_l_.SetKnob(0.5f);
    filter_r_.SetKnob(0.5f);
    dc_l_.Init();
    dc_r_.Init();
}

void DigitalDelay::Reset() {
    digital_line_l.Reset();
    digital_line_r.Reset();
    dc_l_.Init();
    dc_r_.Init();
}

void DigitalDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();
    filter_l_.SetKnob(params.filter);
    filter_r_.SetKnob(params.filter);
}

StereoFrame DigitalDelay::Process(float input, const ParamSet& params) {
    const float base_samps = params.time * SAMPLE_RATE;
    // Add gentle delay-time modulation so Mod Speed/Depth are audible in Digital.
    const float lfo_val    = lfo_out_;
    const float mod_samps  = params.mod_dep * 30.0f;
    
    // Out-of-phase LFO modulation and a 150-sample (~3.1ms) delay offset on the Right
    // channel to create a wide stereo field.
    float delay_l = base_samps + lfo_val * mod_samps;
    float delay_r = base_samps + 150.0f - lfo_val * mod_samps;

    if (delay_l < 1.0f) delay_l = 1.0f;
    if (delay_l > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_l = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }

    if (delay_r < 1.0f) delay_r = 1.0f;
    if (delay_r > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_r = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }

    digital_line_l.SetDelay(delay_l);
    digital_line_r.SetDelay(delay_r);

    float wet_l = digital_line_l.Read();
    float wet_r = digital_line_r.Read();

    wet_l = filter_l_.Process(wet_l);
    wet_r = filter_r_.Process(wet_r);

    const float feedback_l = wet_l * params.repeats;
    const float feedback_r = wet_r * params.repeats;

    digital_line_l.Write(input + feedback_l);
    digital_line_r.Write(input + feedback_r);

    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return StereoFrame{wet_l, wet_r};
}

} // namespace pedal
