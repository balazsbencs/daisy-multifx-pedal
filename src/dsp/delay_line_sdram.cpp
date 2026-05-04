#include "delay_line_sdram.h"
#include <cstring>

namespace pedal {

void DelayLineSdram::Init(float* buf, size_t size) {
    buf_   = buf;
    size_  = size;
    Reset();
}

void DelayLineSdram::Reset() {
    if (buf_) std::memset(buf_, 0, size_ * sizeof(float));
    write_ = 0;
    delay_ = 1;
    frac_  = 0.0f;
}

void DelayLineSdram::SetDelay(float delay_samples) {
    size_t int_part = static_cast<size_t>(delay_samples);
    frac_  = delay_samples - static_cast<float>(int_part);
    if (int_part < 1)     int_part = 1;          // guard zero-latency read
    if (int_part >= size_) int_part = size_ - 1;
    delay_ = int_part;
}

void DelayLineSdram::Write(float sample) {
    buf_[write_] = sample;
    write_ = (write_ == 0) ? size_ - 1 : write_ - 1;
}

float DelayLineSdram::Read() const {
    // write_ < size_ and delay_ < size_, so sums fit in [0, 2*size_-1]:
    // one conditional subtract avoids integer division in the audio ISR.
    size_t a_idx = write_ + delay_;
    if (a_idx >= size_) a_idx -= size_;
    size_t b_idx = a_idx + 1;
    if (b_idx >= size_) b_idx -= size_;
    float a = buf_[a_idx];
    float b = buf_[b_idx];
    return a + (b - a) * frac_;
}

float DelayLineSdram::ReadAt(float delay_samples) const {
    size_t int_part = static_cast<size_t>(delay_samples);
    float  frac     = delay_samples - static_cast<float>(int_part);
    if (int_part >= size_) int_part = size_ - 1;
    size_t a_idx = write_ + int_part;
    if (a_idx >= size_) a_idx -= size_;
    size_t b_idx = a_idx + 1;
    if (b_idx >= size_) b_idx -= size_;
    float a = buf_[a_idx];
    float b = buf_[b_idx];
    return a + (b - a) * frac;
}

} // namespace pedal
