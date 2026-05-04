#pragma once
#include "../config/constants.h"
#include "fast_math.h"

namespace pedal {

enum class LfoWave {
    Sine,           // -1..+1 sinusoidal
    Triangle,       // -1..+1 linear triangle
    Square,         // -1 or +1 (50% duty)
    RampUp,         // -1..+1 sawtooth rising
    RampDown,       // +1..-1 sawtooth falling
    SampleAndHold,  // random step (changes once per cycle)
    Exponential,    // sin²-shaped: slow attack, fast peak approach, slow decay
};

class Lfo {
public:
    void Init(float rate_hz = 1.0f, LfoWave wave = LfoWave::Sine);
    void Reset() { phase_ = phase_offset_; }
    void SetRate(float rate_hz);
    void SetWave(LfoWave wave) { wave_ = wave; }
    void SetPhaseOffset(float offset_radians) {
        static constexpr float TWO_PI = 6.28318530717958647692f;
        // Guard against NaN, Inf, or huge values that would spin the while-loop
        if (offset_radians != offset_radians || offset_radians > 1e6f || offset_radians < -1e6f) {
            phase_offset_ = 0.0f;
            return;
        }
        while (offset_radians >= TWO_PI) offset_radians -= TWO_PI;
        while (offset_radians < 0.0f) offset_radians += TWO_PI;
        phase_offset_ = offset_radians;
    }
    float GetPhase() const { return phase_; }
    // Per-sample: advance phase by one sample, return -1..+1.
    float Process();
    // Per-block: advance phase by BLOCK_SIZE samples, return -1..+1.
    float PrepareBlock();

private:
    float    phase_        = 0.0f;
    float    phase_inc_    = 0.0f;
    float    phase_offset_ = 0.0f;
    float    sh_value_     = 0.0f;  // held value for S&H
    LfoWave  wave_         = LfoWave::Sine;
    uint32_t rand_         = 12345;
};

} // namespace pedal
