#include "fdn.h"
#include "fast_math.h"
#include <cmath>
#include <cstring>

namespace pedal {

// Per-line LFO rates (Hz) — slightly different to prevent synchronization.
static constexpr float kLfoRates[Fdn::MAX_LINES] = {
    0.500f, 0.631f, 0.794f, 1.000f, 1.260f, 1.587f, 2.000f, 2.520f
};

void Fdn::Init(const Config& cfg) {
    n_lines_     = cfg.n_lines < 1 ? 1 : (cfg.n_lines > MAX_LINES ? MAX_LINES : cfg.n_lines);
    sample_rate_ = cfg.sample_rate > 0.0f ? cfg.sample_rate : 48000.0f;

    for (int i = 0; i < n_lines_; ++i) {
        lines_[i].Init(cfg.bufs[i], cfg.delays[i]);
        lines_[i].SetDelay(static_cast<float>(cfg.delays[i] - 1));
        delay_samples_[i] = static_cast<float>(cfg.delays[i] - 1);
        delay_s_[i]       = static_cast<float>(cfg.delays[i]) / sample_rate_;
        lp_state_[i]  = 0.0f;
        lfo_phase_[i] = static_cast<float>(i) / static_cast<float>(n_lines_);
        feedback_[i]  = 0.7f;  // reasonable default
        dc_[i].Init();
    }
}

void Fdn::Reset() {
    for (int i = 0; i < n_lines_; ++i) {
        lines_[i].Reset();
        lp_state_[i] = 0.0f;
        dc_[i].Init();
    }
}

void Fdn::SetDecay(float decay_s) {
    if (decay_s <= 0.0f) decay_s = 0.001f;
    for (int i = 0; i < n_lines_; ++i) {
        // g = 10^(-3 * delay_s / decay_s) = exp(-6.9078 * delay_s / decay_s)
        feedback_[i] = std::exp(-6.9078f * delay_s_[i] / decay_s);
        if (feedback_[i] > 0.9999f) feedback_[i] = 0.9999f;
    }
}

void Fdn::SetDamping(float damp) {
    damp_ = damp < 0.0f ? 0.0f : (damp > 1.0f ? 1.0f : damp);
}

void Fdn::SetModulation(float depth_samples) {
    mod_depth_ = depth_samples < 0.0f ? 0.0f : depth_samples;
}

void Fdn::SetHold(bool hold) {
    hold_ = hold;
}

void Fdn::hadamard4(float v[4]) const {
    const float a = v[0], b = v[1], c = v[2], d = v[3];
    v[0] = (a + b + c + d) * 0.5f;
    v[1] = (a - b + c - d) * 0.5f;
    v[2] = (a + b - c - d) * 0.5f;
    v[3] = (a - b - c + d) * 0.5f;
}

void Fdn::hadamard8(float v[8]) const {
    constexpr float kInvSqrt2 = 0.70710678118f;
    float a[4] = { v[0], v[1], v[2], v[3] };
    float b[4] = { v[4], v[5], v[6], v[7] };
    hadamard4(a);
    hadamard4(b);
    for (int i = 0; i < 4; ++i) {
        v[i]     = (a[i] + b[i]) * kInvSqrt2;
        v[i + 4] = (a[i] - b[i]) * kInvSqrt2;
    }
}

StereoFrame Fdn::Process(float input) {
    float v[MAX_LINES]{};

    // Read from each delay line (with optional LFO modulation).
    const float lfo_inc = 1.0f / sample_rate_;
    for (int i = 0; i < n_lines_; ++i) {
        float delay = delay_samples_[i];
        if (mod_depth_ > 0.0f) {
            const float lfo = fast_sin(lfo_phase_[i] * 6.28318530718f);
            delay += mod_depth_ * lfo;
            if (delay < 1.0f) delay = 1.0f;
        }
        v[i] = lines_[i].ReadAt(delay);

        // Advance LFO phase
        lfo_phase_[i] += kLfoRates[i] * lfo_inc;
        if (lfo_phase_[i] >= 1.0f) lfo_phase_[i] -= 1.0f;
    }

    // One-pole LP damping & DC blocking in feedback path.
    for (int i = 0; i < n_lines_; ++i) {
        float raw_blocked = dc_[i].Process(v[i]);
        lp_state_[i] += damp_ * (raw_blocked - lp_state_[i]);
    }

    // Hadamard mixing.
    float mixed[MAX_LINES]{};
    std::memcpy(mixed, lp_state_, n_lines_ * sizeof(float));
    if (n_lines_ == 8) {
        hadamard8(mixed);
    } else {
        hadamard4(mixed);  // works for n_lines_<=4; ok for n_lines_==4
    }

    // Distribute input and write back with feedback.
    const float input_gain = 1.0f / static_cast<float>(n_lines_);
    for (int i = 0; i < n_lines_; ++i) {
        const float fb = hold_ ? 1.0f : feedback_[i];
        lines_[i].Write(input * input_gain + fb * mixed[i]);
    }

    // Stereo output: even lines → L, odd lines → R.
    float left = 0.0f, right = 0.0f;
    const float out_scale = 2.0f / static_cast<float>(n_lines_);
    for (int i = 0; i < n_lines_; i += 2) left  += v[i];
    for (int i = 1; i < n_lines_; i += 2) right += v[i];
    return StereoFrame{ left * out_scale, right * out_scale };
}

StereoFrame Fdn::Process(StereoFrame input) {
    float v[MAX_LINES]{};

    // Read from each delay line (with optional LFO modulation).
    const float lfo_inc = 1.0f / sample_rate_;
    for (int i = 0; i < n_lines_; ++i) {
        float delay = delay_samples_[i];
        if (mod_depth_ > 0.0f) {
            const float lfo = fast_sin(lfo_phase_[i] * 6.28318530718f);
            delay += mod_depth_ * lfo;
            if (delay < 1.0f) delay = 1.0f;
        }
        v[i] = lines_[i].ReadAt(delay);

        // Advance LFO phase
        lfo_phase_[i] += kLfoRates[i] * lfo_inc;
        if (lfo_phase_[i] >= 1.0f) lfo_phase_[i] -= 1.0f;
    }

    // One-pole LP damping & DC blocking in feedback path.
    for (int i = 0; i < n_lines_; ++i) {
        float raw_blocked = dc_[i].Process(v[i]);
        lp_state_[i] += damp_ * (raw_blocked - lp_state_[i]);
    }

    // Hadamard mixing.
    float mixed[MAX_LINES]{};
    std::memcpy(mixed, lp_state_, n_lines_ * sizeof(float));
    if (n_lines_ == 8) {
        hadamard8(mixed);
    } else {
        hadamard4(mixed);  // works for n_lines_<=4; ok for n_lines_==4
    }

    // Distribute input and write back with feedback.
    const float input_gain = 1.0f / static_cast<float>(n_lines_);
    for (int i = 0; i < n_lines_; ++i) {
        const float fb = hold_ ? 1.0f : feedback_[i];
        const float in_val = (i % 2 == 0) ? input.left : input.right;
        lines_[i].Write(in_val * input_gain + fb * mixed[i]);
    }

    // Stereo output: even lines → L, odd lines → R.
    float left = 0.0f, right = 0.0f;
    const float out_scale = 2.0f / static_cast<float>(n_lines_);
    for (int i = 0; i < n_lines_; i += 2) left  += v[i];
    for (int i = 1; i < n_lines_; i += 2) right += v[i];
    return StereoFrame{ left * out_scale, right * out_scale };
}

} // namespace pedal
