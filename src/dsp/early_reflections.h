#pragma once
#include <cstddef>
#include <cstdint>
#include "delay_line_sdram.h"
#include "../audio/stereo_frame.h"

namespace pedal {

struct ErTap {
    uint16_t delay_samples;
    float    gain;
    float    pan;  // -1..+1 (left=-1, center=0, right=+1)
};

class EarlyReflections {
public:
    static constexpr int MAX_TAPS = 48;

    void Init(float* buf, size_t size) { line_.Init(buf, size); }
    void Reset() { line_.Reset(); }

    void SetTaps(const ErTap* taps, int count) {
        tap_count_ = count < MAX_TAPS ? count : MAX_TAPS;
        for (int i = 0; i < tap_count_; ++i) taps_[i] = taps[i];
    }

    StereoFrame Process(float input) {
        StereoFrame out{};
        for (int i = 0; i < tap_count_; ++i) {
            const float s     = line_.ReadAt(static_cast<float>(taps_[i].delay_samples));
            const float g     = taps_[i].gain;
            const float p     = taps_[i].pan;
            out.left  += s * g * 0.5f * (1.0f - p);
            out.right += s * g * 0.5f * (1.0f + p);
        }
        line_.Write(input);
        return out;
    }

private:
    DelayLineSdram line_;
    ErTap          taps_[MAX_TAPS]{};
    int            tap_count_ = 0;
};

} // namespace pedal
