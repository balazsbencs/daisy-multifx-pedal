#include "filter_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../dsp/fast_math.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS filter_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       filter_line;

void FilterDelay::Init() {
    filter_line.Init(filter_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    dc_.Init();
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void FilterDelay::Reset() {
    filter_line.Reset();
    dc_.Init();
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void FilterDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    lfo_out_ = lfo_.PrepareBlock();

    // Move all block-stable and LFO-dependent computations here so
    // Process() runs no transcendental functions per sample.
    float delay_samps = params.time * SAMPLE_RATE;
    if (delay_samps > static_cast<float>(MAX_DELAY_SAMPLES - 1))
        delay_samps = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    filter_line.SetDelay(delay_samps);

    float cutoff_hz = 400.0f + lfo_out_ * params.mod_dep * 2000.0f;
    if (cutoff_hz < 40.0f)    cutoff_hz = 40.0f;
    if (cutoff_hz > 10000.0f) cutoff_hz = 10000.0f;

    svf_f_ = 2.0f * fast_sin(3.14159265f * cutoff_hz * INV_SAMPLE_RATE);
    if (svf_f_ > 1.7f) svf_f_ = 1.7f;

    // Damping q: 0 = self-oscillate, 2 = critically damped.
    svf_q_ = 2.0f - params.mod_dep * 1.95f;
}

StereoFrame FilterDelay::Process(float input, const ParamSet& params) {
    float wet = filter_line.Read();

    // State-variable filter (Chamberlin); coefficients set once per block in Prepare()
    const float hp = wet - svf_q_ * z1_ - z2_;
    const float bp = svf_f_ * hp + z1_;
    z1_            = bp;
    const float lp = svf_f_ * bp + z2_;
    z2_            = lp;

    // Output is low-pass
    wet = lp;

    // Hard-clamp before feedback: near self-oscillation (low q) the SVF output
    // can far exceed ±1.0, and writing that back into the delay line without a
    // guard causes exponential runaway.
    if (wet >  1.0f) wet =  1.0f;
    if (wet < -1.0f) wet = -1.0f;

    const float feedback = wet * params.repeats;
    filter_line.Write(input + feedback);

    wet = dc_.Process(wet);

    return StereoFrame{wet, wet};
}

} // namespace pedal
