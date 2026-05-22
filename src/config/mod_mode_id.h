#pragma once
#include <cstdint>

namespace pedal {

enum class ModModeId : uint8_t {
    Chorus      = 0,
    Flanger     = 1,
    Rotary      = 2,
    Vibe        = 3,
    Phaser      = 4,
    VintTrem    = 5,
    PatternTrem = 6,
    AutoSwell   = 7,
    FilterFx    = 8,
    FormantFx   = 9,
    Quadrature  = 10,
    Destroyer   = 11,
    COUNT       = 12,
};

} // namespace pedal
