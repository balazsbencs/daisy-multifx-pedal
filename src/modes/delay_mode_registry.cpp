#include "delay_mode_registry.h"
#include "digital_delay.h"
#include "tape_delay.h"
#include "dual_delay.h"
#include "filter_delay.h"

namespace pedal {

// Static instances: objects are small (DSP state only); SDRAM buffers are in
// their respective .cpp files.  No heap allocation anywhere.
static DigitalDelay s_digital;
static TapeDelay    s_tape;
static DualDelay    s_dual;
static FilterDelay  s_filter;

void DelayModeRegistry::Init() {
    modes_[static_cast<uint8_t>(DelayModeId::Digital)] = &s_digital;
    modes_[static_cast<uint8_t>(DelayModeId::Tape)]    = &s_tape;
    modes_[static_cast<uint8_t>(DelayModeId::Dual)]    = &s_dual;
    modes_[static_cast<uint8_t>(DelayModeId::Filter)]  = &s_filter;

    for (int i = 0; i < NUM_DELAY_MODES; ++i) {
        modes_[i]->Init();
    }
}

DelayMode* DelayModeRegistry::get(DelayModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void DelayModeRegistry::Reset(DelayModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal
