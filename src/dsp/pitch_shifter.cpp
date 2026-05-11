#include "pitch_shifter.h"
#include <cmath>
#include <cstring>

namespace pedal {

void PitchShifter::Init(float* buf, size_t buf_size, float /*sample_rate*/) {
    buf_      = buf;
    buf_size_ = buf_size > 0 ? buf_size : GRAIN_SIZE * 2;
    if (buf_) std::memset(buf_, 0, buf_size_ * sizeof(float));
    write_pos_     = 0;
    read_pos_[0]   = 0.0f;
    read_pos_[1]   = static_cast<float>(GRAIN_SIZE) * 0.5f;  // 50% offset
    grain_phase_[0] = 0.0f;
    grain_phase_[1] = 0.5f;
    ratio_         = 1.0f;
}

void PitchShifter::Reset() {
    if (buf_) std::memset(buf_, 0, buf_size_ * sizeof(float));
    write_pos_      = 0;
    read_pos_[0]    = 0.0f;
    read_pos_[1]    = static_cast<float>(GRAIN_SIZE) * 0.5f;
    grain_phase_[0] = 0.0f;
    grain_phase_[1] = 0.5f;
}

void PitchShifter::SetShift(float semitones) {
    ratio_ = std::pow(2.0f, semitones / 12.0f);
    const float max_ratio = static_cast<float>(buf_size_) / static_cast<float>(GRAIN_SIZE);
    if (ratio_ > max_ratio) ratio_ = max_ratio;
}

float PitchShifter::ReadInterp(float pos) const {
    const float sz = static_cast<float>(buf_size_);
    while (pos >= sz) pos -= sz;

    const size_t i0   = static_cast<size_t>(pos);
    const float  frac = pos - static_cast<float>(i0);
    const size_t i1   = (i0 + 1 < buf_size_) ? i0 + 1 : 0;
    return buf_[i0] * (1.0f - frac) + buf_[i1] * frac;
}

float PitchShifter::Process(float input) {
    if (!buf_) return input;

    buf_[write_pos_] = input;
    write_pos_ = (write_pos_ + 1 < buf_size_) ? write_pos_ + 1 : 0;

    float out = 0.0f;
    const float phase_inc = 1.0f / static_cast<float>(GRAIN_SIZE);

    for (int g = 0; g < 2; ++g) {
        // Hann window: peaks at phase=0.5, zero at phase=0 and 1
        const float w = 0.5f * (1.0f - std::cos(grain_phase_[g] * 6.28318530718f));
        out += w * ReadInterp(read_pos_[g]);

        read_pos_[g]   += ratio_;
        grain_phase_[g] += phase_inc;

        if (grain_phase_[g] >= 1.0f) {
            grain_phase_[g] -= 1.0f;
            read_pos_[g] = static_cast<float>(write_pos_);
        }
    }
    return out;
}

} // namespace pedal
