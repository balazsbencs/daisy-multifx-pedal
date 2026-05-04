#include "rotary_mode.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::mod_fx;

namespace pedal {

// Horn: max ~5ms = 240 samples; Drum: max ~10ms = 480 samples
static constexpr size_t kHornBufSize = 256;
static constexpr size_t kDrumBufSize = 512;
static float DSY_SDRAM_BSS s_horn_buf[kHornBufSize];
static float DSY_SDRAM_BSS s_drum_buf[kDrumBufSize];
static DelayLineSdram s_horn_line;
static DelayLineSdram s_drum_line;

static constexpr float TWO_PI = 6.28318530717958647692f;

void RotaryMode::Init() {
    s_horn_line.Init(s_horn_buf, kHornBufSize);
    s_drum_line.Init(s_drum_buf, kDrumBufSize);

    horn_lfo_.Init(1.8f, LfoWave::Sine);
    horn_lfo_q_.Init(1.8f, LfoWave::Sine);
    horn_lfo_q_.SetPhaseOffset(TWO_PI * 0.25f); // 90°
    horn_lfo_q_.Reset();  // apply offset to phase_

    drum_lfo_.Init(1.0f, LfoWave::Sine);
    drum_lfo_q_.Init(1.0f, LfoWave::Sine);
    drum_lfo_q_.SetPhaseOffset(TWO_PI * 0.25f);
    drum_lfo_q_.Reset();  // apply offset to phase_

    drive_.Init();
    dc_l_.Init();
    dc_r_.Init();
}

void RotaryMode::Reset() {
    s_horn_line.Reset();
    s_drum_line.Reset();
    horn_lfo_.Reset();
    horn_lfo_q_.Reset();
    drum_lfo_.Reset();
    drum_lfo_q_.Reset();
    dc_l_.Init();
    dc_r_.Init();
    xover_state_ = 0.0f;
}

void RotaryMode::Prepare(const ParamSet& params) {
    // Horn rate: speed param; Drum rate: speed * 0.56 (Leslie ratio)
    const float horn_rate = params.speed;
    const float drum_rate = params.speed * 0.56f;

    horn_lfo_.SetRate(horn_rate);
    horn_lfo_q_.SetRate(horn_rate);
    drum_lfo_.SetRate(drum_rate);
    drum_lfo_q_.SetRate(drum_rate);

    // Drive from P1
    drive_.SetDrive(params.p1);

    // Cache depth-derived values for per-sample use in Process().
    am_depth_ = params.depth * 0.4f;
    horn_mod_ = params.depth * 100.0f;  // max ±100 samples = ~2ms
    drum_mod_ = params.depth * 200.0f;  // max ±200 samples = ~4ms
    // LFO values computed per-sample in Process() to avoid block-boundary zipper noise.
}

StereoFrame RotaryMode::Process(StereoFrame input, const ParamSet& params) {
    // Optional drive stage
    float driven = (params.p1 > 0.02f) ? drive_.Process(input.mono()) : input.mono();

    // Simple 1-pole crossover: LP → drum, remainder → horn
    // tone: 0=low crossover (~500Hz), 1=high crossover (~3kHz)
    const float xover = 0.05f + params.tone * 0.4f;
    xover_state_ += xover * (driven - xover_state_);
    const float drum_in = xover_state_;
    const float horn_in = driven - drum_in;

    // Per-sample LFO values for smooth Doppler + AM modulation.
    const float hl  = horn_lfo_.Process();
    const float hlq = horn_lfo_q_.Process();
    const float dl  = drum_lfo_.Process();
    const float dlq = drum_lfo_q_.Process();

    // AM coefficients
    const float horn_am_l = 1.0f - am_depth_ * (0.5f + 0.5f * hl);
    const float horn_am_r = 1.0f - am_depth_ * (0.5f + 0.5f * hlq);
    const float drum_am_l = 1.0f - am_depth_ * 0.7f * (0.5f + 0.5f * dl);
    const float drum_am_r = 1.0f - am_depth_ * 0.7f * (0.5f + 0.5f * dlq);

    // Doppler delays — clamp to valid range.
    float horn_delay = 120.0f + hl * horn_mod_;
    float drum_delay = 240.0f + dl * drum_mod_;
    if (horn_delay < 1.0f) horn_delay = 1.0f;
    if (horn_delay >= static_cast<float>(kHornBufSize - 1))
        horn_delay = static_cast<float>(kHornBufSize - 1);
    if (drum_delay < 1.0f) drum_delay = 1.0f;
    if (drum_delay >= static_cast<float>(kDrumBufSize - 1))
        drum_delay = static_cast<float>(kDrumBufSize - 1);

    // Horn delay (Doppler)
    s_horn_line.SetDelay(horn_delay);
    s_horn_line.Write(horn_in);
    const float horn_wet = s_horn_line.Read();

    // Drum delay (Doppler)
    s_drum_line.SetDelay(drum_delay);
    s_drum_line.Write(drum_in);
    const float drum_wet = s_drum_line.Read();

    // P2: horn/drum balance (0=all drum, 1=all horn)
    const float horn_mix = params.p2;
    const float drum_mix = 1.0f - params.p2;

    float out_l = horn_wet * horn_am_l * horn_mix + drum_wet * drum_am_l * drum_mix;
    float out_r = horn_wet * horn_am_r * horn_mix + drum_wet * drum_am_r * drum_mix;

    out_l = dc_l_.Process(out_l);
    out_r = dc_r_.Process(out_r);
    return {out_l, out_r};
}

} // namespace pedal
