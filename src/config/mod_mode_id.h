#pragma once
#include <cstdint>

namespace pedal {

enum class ModModeId : uint8_t {
    Chorus   = 0,
    Flanger  = 1,
    Rotary   = 2,
    Vibe     = 3,
    Phaser   = 4,
    VintTrem = 5,
    COUNT    = 6,
};

} // namespace pedal
