#include "tape_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS tape_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       tape_line;

void TapeDelay::Init() {
    tape_line.Init(tape_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::SmoothRandom);
    lfo_.SetJitter(0.5f);
    filter_.Init();
    filter_.SetKnob(0.4f); // slight LP for tape warmth default
    sat_.Init(WaveCurve::Tape);
    dc_.Init();
}

void TapeDelay::Reset() {
    lfo_.Reset();
    tape_line.Reset();
    dc_.Init();
}

void TapeDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();
    filter_.SetKnob(params.filter);
    sat_.SetDrive(params.grit);
}

StereoFrame TapeDelay::Process(float input, const ParamSet& params) {
    const float lfo_val = lfo_out_;

    // wow/flutter: max deviation = mod_dep * 50 samples
    const float flutter     = params.mod_dep * 50.0f;
    const float base_samps  = params.time * SAMPLE_RATE;
    float delay_samps       = base_samps + lfo_val * flutter;
    if (delay_samps < 1.0f)                         delay_samps = 1.0f;
    if (delay_samps > static_cast<float>(MAX_DELAY_SAMPLES - 1))
        delay_samps = static_cast<float>(MAX_DELAY_SAMPLES - 1);

    tape_line.SetDelay(delay_samps);

    float wet = tape_line.Read();

    wet = filter_.Process(wet);

    // Saturation on feedback path; drive scales with grit
    wet = sat_.Process(wet);

    const float feedback = wet * params.repeats;
    float write_val = input + feedback;
    if (write_val >  1.0f) write_val =  1.0f;
    if (write_val < -1.0f) write_val = -1.0f;
    tape_line.Write(write_val);

    wet = dc_.Process(wet);

    return StereoFrame{wet, wet};
}

} // namespace pedal
