#include "digital_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS digital_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       digital_line;

void DigitalDelay::Init() {
    digital_line.Init(digital_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    filter_.Init();
    filter_.SetKnob(0.5f);
    dc_.Init();
}

void DigitalDelay::Reset() {
    digital_line.Reset();
    dc_.Init();
}

void DigitalDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();
    filter_.SetKnob(params.filter);
}

StereoFrame DigitalDelay::Process(float input, const ParamSet& params) {
    const float base_samps = params.time * SAMPLE_RATE;
    // Add gentle delay-time modulation so Mod Speed/Depth are audible in Digital.
    const float lfo_val    = lfo_out_;
    const float mod_samps  = params.mod_dep * 30.0f;
    float delay_samps      = base_samps + lfo_val * mod_samps;
    if (delay_samps < 1.0f) {
        delay_samps = 1.0f;
    }
    if (delay_samps > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_samps = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }
    digital_line.SetDelay(delay_samps);

    float wet = digital_line.Read();
    wet = filter_.Process(wet);

    const float feedback = wet * params.repeats;
    digital_line.Write(input + feedback);

    wet = dc_.Process(wet);

    return StereoFrame{wet, wet};
}

} // namespace pedal
