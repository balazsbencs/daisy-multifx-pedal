#pragma once
#include <cstddef>
#include <cstring>
#include "daisy_seed.h"

namespace pedal {

class DelayLineSdram {
public:
    DelayLineSdram() = default;

    // buf must point to a DSY_SDRAM_BSS array; size is number of floats
    void Init(float* buf, size_t size);
    void Reset();

    void  SetDelay(float delay_samples); // fractional, linear interp
    void  Write(float sample);
    float Read() const;
    float ReadAt(float delay_samples) const; // read arbitrary tap

private:
    float*  buf_   = nullptr;
    size_t  size_  = 0;
    size_t  write_ = 0;
    size_t  delay_ = 1;
    float   frac_  = 0.0f;
};

} // namespace pedal
