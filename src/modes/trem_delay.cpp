#include "trem_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS trem_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       trem_line;

void TremDelay::Init() {
    trem_line.Init(trem_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    filter_.Init();
    filter_.SetKnob(0.5f);
    dc_.Init();
}

void TremDelay::Reset() {
    trem_line.Reset();
    dc_.Init();
}

void TremDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();
    filter_.SetKnob(params.filter);
}

StereoFrame TremDelay::Process(float input, const ParamSet& params) {
    const float delay_samps = params.time * SAMPLE_RATE;
    trem_line.SetDelay(delay_samps);

    const float lfo_val = lfo_out_;

    // gain = 1 - depth * (1 - lfo) / 2
    // At lfo=+1: gain = 1 (full amplitude)
    // At lfo=-1: gain = 1 - depth (most reduced)
    const float gain = 1.0f - params.mod_dep * (1.0f - lfo_val) * 0.5f;

    float wet = trem_line.Read();
    wet = filter_.Process(wet);

    // Feedback is taken BEFORE tremolo modulation
    const float feedback = wet * params.repeats;
    trem_line.Write(input + feedback);

    // Apply tremolo to the output mix only
    wet *= gain;
    wet = dc_.Process(wet);

    return StereoFrame{wet, wet};
}

} // namespace pedal
