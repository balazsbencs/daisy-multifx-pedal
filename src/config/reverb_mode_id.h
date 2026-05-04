#pragma once
#include <cstdint>

namespace pedal {

enum class ReverbModeId : uint8_t {
    Room   = 0,
    Hall   = 1,
    Plate  = 2,
    Spring = 3,
    COUNT  = 4,
};

} // namespace pedal
