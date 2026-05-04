#include "vintage_trem_mode.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

void VintageTremMode::Init() {
    Reset();
}

void VintageTremMode::Reset() {
    lfo_.Init(4.0f, LfoWave::Sine);
    depth_ = 0.5f;
}

void VintageTremMode::Prepare(const ParamSet& params) {
    // sub-mode from p2: 0=Tube (sine), 1=Harmonic (triangle), 2=Photoresistor (exponential)
    const int sub = static_cast<int>(params.p2 * 2.999f);
    if (sub == 0)      lfo_.SetWave(LfoWave::Sine);
    else if (sub == 1) lfo_.SetWave(LfoWave::Triangle);
    else               lfo_.SetWave(LfoWave::Exponential);

    lfo_.SetRate(params.speed);
    depth_ = params.depth;
    // Gain computed per-sample in Process() to avoid block-boundary zipper noise.
}

StereoFrame VintageTremMode::Process(StereoFrame input, const ParamSet& /*params*/) {
    // Per-sample LFO for smooth amplitude modulation.
    const float lfo_val = lfo_.Process(); // -1..+1
    float gain = 1.0f - depth_ * (0.5f + 0.5f * lfo_val);
    if (gain < 0.0f) gain = 0.0f;
    return {input.left * gain, input.right * gain};
}

} // namespace pedal
