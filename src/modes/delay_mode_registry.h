#pragma once
#include "delay_mode.h"
#include "../config/delay_mode_id.h"
#include "../config/constants.h"

namespace pedal {

class DelayModeRegistry {
public:
    void Init();
    DelayMode* get(DelayModeId id);
    void Reset(DelayModeId id);

private:
    DelayMode* modes_[NUM_DELAY_MODES]{};
};

} // namespace pedal
