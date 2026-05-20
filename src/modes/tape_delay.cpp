#include "tape_delay.h"
#include "../dsp/delay_line_sdram.h"
#include "../config/constants.h"

using namespace pedal::delay_fx;

namespace pedal {

static constexpr float kStereoOffsetSamples = 150.0f;

static float DSY_SDRAM_BSS tape_buf[MAX_DELAY_SAMPLES];
static DelayLineSdram       tape_line;

void TapeDelay::Init() {
    tape_line.Init(tape_buf, MAX_DELAY_SAMPLES);
    lfo_.Init(1.0f, LfoWave::SmoothRandom);
    lfo_.SetJitter(0.5f);
    filter_.Init();
    filter_.SetKnob(0.4f); // slight LP for tape warmth default
    sat_.Init(WaveCurve::Tape);
    dc_l_.Init();
    dc_r_.Init();
    env_state_ = 0.0f;
    tape_lp_ = 0.0f;
}

void TapeDelay::Reset() {
    lfo_.Reset();
    lfo_out_ = 0.0f;
    tape_line.Reset();
    dc_l_.Init();
    dc_r_.Init();
    env_state_ = 0.0f;
    tape_lp_ = 0.0f;
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
    if (delay_samps < 1.0f) delay_samps = 1.0f;
    if (delay_samps > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_samps = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }

    tape_line.SetDelay(delay_samps);

    // Read Left tap (primary play head)
    float wet_l = tape_line.Read();

    // Read Right tap (secondary play head, offset by 150 samples / ~3.1ms)
    float delay_r = delay_samps + kStereoOffsetSamples;
    if (delay_r > static_cast<float>(MAX_DELAY_SAMPLES - 1)) {
        delay_r = static_cast<float>(MAX_DELAY_SAMPLES - 1);
    }
    float wet_r = tape_line.ReadAt(delay_r);

    // Feedback is mono-summed and processed through tape color & dynamic HF limiter
    float fb_mono = 0.5f * (wet_l + wet_r);
    fb_mono = filter_.Process(fb_mono);
    fb_mono = sat_.Process(fb_mono);

    // Dynamic HF compression (tape HF demagnetization & saturation):
    // Envelope follower tracks feedback level
    const float env = (fb_mono > 0.0f) ? fb_mono : -fb_mono;
    env_state_ += 0.05f * (env - env_state_);

    // Lower cutoff coefficient (more low-pass) as volume/grit increases
    float k_lp = 0.45f - 0.35f * env_state_ * params.grit;
    if (k_lp < 0.05f) k_lp = 0.05f;
    if (k_lp > 0.45f) k_lp = 0.45f;

    tape_lp_ += k_lp * (fb_mono - tape_lp_);
    fb_mono = tape_lp_;

    // Apply gain compensation to keep loop gain controlled and prevent runaway feedback scream,
    // while still allowing warm, organic self-oscillation at maximum repeats/grit.
    const float comp = 1.0f / (1.0f + params.grit * 12.33f);
    fb_mono *= comp;

    const float feedback = fb_mono * params.repeats;
    float write_val = input + feedback;
    if (write_val >  1.0f) write_val =  1.0f;
    if (write_val < -1.0f) write_val = -1.0f;
    tape_line.Write(write_val);

    // Apply DC blockers independently per channel
    wet_l = dc_l_.Process(wet_l);
    wet_r = dc_r_.Process(wet_r);

    return StereoFrame{wet_l, wet_r};
}

} // namespace pedal
