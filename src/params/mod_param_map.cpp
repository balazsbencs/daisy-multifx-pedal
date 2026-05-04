#include "mod_param_map.h"

namespace pedal {
namespace mod_fx {

namespace mode_ranges {
    constexpr ParamRange SPEED_VINTTREM = {1.0f, 15.0f, 1.0f};
}

const ParamRange& get_param_range(ModModeId mode, ParamId param) {
    if (param == ParamId::Speed) {
        switch (mode) {
            case ModModeId::VintTrem: return mode_ranges::SPEED_VINTTREM;
            default: break;
        }
    }
    switch (param) {
        case ParamId::Speed: return default_ranges::SPEED;
        case ParamId::Depth: return default_ranges::DEPTH;
        case ParamId::Mix:   return default_ranges::MIX;
        case ParamId::Tone:  return default_ranges::TONE;
        case ParamId::P1:    return default_ranges::P1;
        case ParamId::P2:    return default_ranges::P2;
        case ParamId::Level: return default_ranges::LEVEL;
        default:             return default_ranges::MIX;
    }
}

} // namespace mod_fx
} // namespace pedal
