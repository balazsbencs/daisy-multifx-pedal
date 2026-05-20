#include "reverb_param_map.h"

namespace pedal {
namespace reverb_fx {

const ParamRange& get_param_range(ReverbModeId mode, ParamId param) {
    switch (param) {
        case ParamId::Decay:
            switch (mode) {
                case ReverbModeId::Room:   return default_ranges::DECAY_ROOM;
                case ReverbModeId::Spring: return default_ranges::DECAY_SPRING;
                default:                   return default_ranges::DECAY;
            }
        case ParamId::PreDelay:  return default_ranges::PRE_DELAY;
        // TODO: when ReverbModeId::Shimmer is added to the enum, override Param1/Param2 here:
        //   if (mode == ReverbModeId::Shimmer && param == ParamId::Param1) return default_ranges::PARAM1_SHIMMER;
        //   if (mode == ReverbModeId::Shimmer && param == ParamId::Param2) return default_ranges::PARAM2_SHIMMER;
        case ParamId::Param1:    return default_ranges::PARAM1;
        case ParamId::Param2:    return default_ranges::PARAM2;
        case ParamId::Mix:  return default_ranges::MIX;
        case ParamId::Tone: return default_ranges::TONE;
        case ParamId::Mod:  return default_ranges::MOD;
        default:            return default_ranges::MIX;
    }
}

const AlgoParamDescriptor& get_algo_param_descriptor(ReverbModeId mode) {
    static const AlgoParamDescriptor kDescriptors[4] = {
        {"Size",  "Diffusion"},
        {"Size",  "Mid EQ"},
        {"Size",  ""},
        {"Dwell", "Springs"},
    };
    const int idx = static_cast<int>(mode);
    return kDescriptors[idx < 4 ? idx : 0];
}

} // namespace reverb_fx
} // namespace pedal
