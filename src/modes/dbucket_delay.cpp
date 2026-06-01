#include "dbucket_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"
#include <cstdint>

using namespace pedal::delay_fx;

namespace pedal {

static float DSY_SDRAM_BSS dbucket_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       dbucket_line;

void DbucketDelay::Init() {
    dbucket_line.Init(dbucket_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::Sine);
    filter_.Init();
    filter_.SetKnob(0.4f);
    dc_.Init();
    bbd_.Reset();
    noise_seed_   = 12345u;
    delay_smooth_ = 0.0f;
}

void DbucketDelay::Reset() {
    dbucket_line.Reset();
    dc_.Init();
    bbd_.Reset();
    noise_seed_   = 12345u;
    delay_smooth_ = 0.0f;
}

void DbucketDelay::Prepare(const ParamSet& params) {
    lfo_.SetRate(params.mod_spd);
    const float filter_knob = 0.4f - params.grit * 0.3f;
    filter_.SetKnob(filter_knob);
}

StereoFrame DbucketDelay::Process(float input, const ParamSet& params) {
    static constexpr float kDelaySlew = 0.0001f;  // BBD clock change glides pitch

    const float base_samps = params.time * SAMPLE_RATE;
    delay_smooth_ += kDelaySlew * (base_samps - delay_smooth_);

    const float lfo_val   = lfo_.Process();
    float delay_samps     = delay_smooth_ + lfo_val * (params.mod_dep * 20.0f);
    if (delay_samps < 1.0f)
        delay_samps = 1.0f;
    if (delay_samps > static_cast<float>(MAX_DELAY_SAMPLES - 1))
        delay_samps = static_cast<float>(MAX_DELAY_SAMPLES - 1);

    dbucket_line.SetDelay(delay_samps);

    float wet = dbucket_line.Read();
    wet = bbd_.Deemphasis(wet);
    wet = filter_.Process(wet);

    const float feedback = wet * params.repeats;
    float write_in = bbd_.Process(input + feedback, params.grit, noise_seed_, delay_samps);
    dbucket_line.Write(write_in);

    wet = dc_.Process(wet);
    return StereoFrame{wet, wet};
}

} // namespace pedal
