#pragma once
#include "allpass.h"

namespace pedal {

namespace detail {
constexpr size_t kDiffuserDelays[4] = {142, 107, 379, 277};
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

    // d: 0..1 → allpass g coefficient 0..0.7
    void SetDiffusion(float d) { g_ = d * 0.7f; }

    float Process(float input) {
        float s = input;
        for (auto& stage : stages_) s = stage.Process(s, g_);
        return s;
    }

private:
    DelayAllpassFilter stages_[STAGES];
    float              g_ = 0.5f;
};

} // namespace pedal
