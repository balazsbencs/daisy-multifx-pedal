#pragma once
#include "allpass.h"

namespace pedal {

// 4-stage allpass cascade for smearing transients.
class Diffuser {
public:
    static constexpr int    STAGES = 4;
    // Mutually co-prime, Fibonacci-ratio spacing (each ≈ 1.618× previous).
    // All four are prime → GCD of any pair = 1 → even modal distribution.
    static constexpr size_t kDelays[STAGES] = {149, 241, 389, 631};

    void Init(float* bufs[STAGES], const size_t sizes[STAGES]) {
        for (int i = 0; i < STAGES; ++i) {
            stages_[i].Init(bufs[i], sizes[i]);
            stages_[i].SetDelay(kDelays[i]);
        }
    }

    void Reset() {
        for (auto& s : stages_) s.Reset();
    }

    // d: 0..1
    // |g| must remain < 1 for allpass stability.
    // First pair (0,1) use lower g to reduce spectral coloration.
    // Second pair (2,3) use slightly higher g for better diffusion depth.
    void SetDiffusion(float d) {
        g_[0] = 0.50f + d * 0.10f;  // 0.50 – 0.60
        g_[1] = 0.50f + d * 0.10f;  // 0.50 – 0.60
        g_[2] = 0.55f + d * 0.15f;  // 0.55 – 0.70
        g_[3] = 0.55f + d * 0.15f;  // 0.55 – 0.70
    }

    float Process(float input) {
        float s = input;
        for (int i = 0; i < STAGES; ++i) s = stages_[i].Process(s, g_[i]);
        return s;
    }

private:
    DelayAllpassFilter stages_[STAGES];
    // Defaults correspond to SetDiffusion(0.0f) — minimum operating point.
    float              g_[STAGES] = {0.50f, 0.50f, 0.55f, 0.55f};
};

} // namespace pedal
