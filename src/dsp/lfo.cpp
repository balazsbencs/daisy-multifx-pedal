#include "lfo.h"

namespace pedal {

static constexpr float TWO_PI = 6.28318530717958647692f;
static constexpr float PI     = 3.14159265358979323846f;

void Lfo::Init(float rate_hz, LfoWave wave) {
    phase_        = 0.0f;
    phase_offset_ = 0.0f;
    sh_value_     = 0.0f;
    rand_         = 12345;
    wave_         = wave;
    SetRate(rate_hz);
}

void Lfo::SetRate(float rate_hz) {
    phase_inc_ = TWO_PI * rate_hz * INV_SAMPLE_RATE;
}

static float lfo_compute(float phase, LfoWave wave) {
    switch (wave) {
        case LfoWave::Sine:
            return fast_sin(phase);
        case LfoWave::Triangle:
            return (phase < PI)
                ? (-1.0f + phase * (2.0f / PI))
                : ( 3.0f - phase * (2.0f / PI));
        case LfoWave::Square:
            return (phase < PI) ? 1.0f : -1.0f;
        case LfoWave::RampUp:
            return -1.0f + phase * (2.0f / TWO_PI);
        case LfoWave::RampDown:
            return 1.0f - phase * (2.0f / TWO_PI);
        case LfoWave::Exponential: {
            const float s = fast_sin(phase);
            return (s >= 0.0f) ? (s * s) : -(s * s);
        }
        default:
            return 0.0f;
    }
}

float Lfo::Process() {
    float out;
    if (wave_ == LfoWave::SampleAndHold) {
        out = sh_value_;
    } else {
        out = lfo_compute(phase_, wave_);
    }
    phase_ += phase_inc_;
    // Normalize phase to [0, 2π) for both positive and negative increments
    while (phase_ >= TWO_PI) {
        phase_ -= TWO_PI;
        // New S&H sample on cycle wrap
        rand_     = rand_ * 1664525u + 1013904223u;
        sh_value_ = static_cast<float>(static_cast<int32_t>(rand_)) * (1.0f / 2147483648.0f);
    }
    while (phase_ < 0.0f) {
        phase_ += TWO_PI;
        rand_     = rand_ * 1664525u + 1013904223u;
        sh_value_ = static_cast<float>(static_cast<int32_t>(rand_)) * (1.0f / 2147483648.0f);
    }
    return out;
}

float Lfo::PrepareBlock() {
    float out;
    if (wave_ == LfoWave::SampleAndHold) {
        out = sh_value_;
    } else {
        out = lfo_compute(phase_, wave_);
    }
    phase_ += phase_inc_ * static_cast<float>(BLOCK_SIZE);
    // Normalize phase to [0, 2π) for both positive and negative increments
    while (phase_ >= TWO_PI) {
        phase_ -= TWO_PI;
        rand_     = rand_ * 1664525u + 1013904223u;
        sh_value_ = static_cast<float>(static_cast<int32_t>(rand_)) * (1.0f / 2147483648.0f);
    }
    while (phase_ < 0.0f) {
        phase_ += TWO_PI;
        rand_     = rand_ * 1664525u + 1013904223u;
        sh_value_ = static_cast<float>(static_cast<int32_t>(rand_)) * (1.0f / 2147483648.0f);
    }
    return out;
}

} // namespace pedal
