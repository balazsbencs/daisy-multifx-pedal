#pragma once
#include <cstdint>

namespace pedal {

enum class DelayModeId : uint8_t {
    Digital = 0,
    Tape    = 1,
    Dual    = 2,
    Filter  = 3,
    COUNT   = 4,
};

} // namespace pedal
