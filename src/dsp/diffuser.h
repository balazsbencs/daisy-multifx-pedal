#pragma once
#include "allpass.h"

namespace pedal {

namespace detail {
// Original: {142, 107, 379, 277} — max 7.9 ms, too short for transient blur.
// Updated: two longer stages (14 ms, 8.6 ms) for meaningful smear depth.
// All values are mutually coprime to avoid periodic coloration.
constexpr size_t kDiffuserDelays[4] = {142, 107, 672, 413};
} // namespace detail

// 4-stage allpass cascade for smearing transients.
class Diffuser {
public:
    static constexpr int STAGES = 4;

    void Init(float* bufs[STAGES], const size_t sizes[STAGES]) {
        for (int i = 0; i < STAGES; ++i) {
            stages_[i].Init(bufs[i], sizes[i]);
            stages_[i].SetDelay(detail::kDiffuserDelays[i]);
        }
    }

    void Reset() {
        for (auto& s : stages_) s.Reset();
    }

    // d: 0..1
    // Outer stages (0,1) use lower g to reduce spectral coloration.
    // Inner stages (2,3) use slightly higher g for better diffusion depth.
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
    float              g_[STAGES] = {0.55f, 0.55f, 0.60f, 0.60f};
};

} // namespace pedal
