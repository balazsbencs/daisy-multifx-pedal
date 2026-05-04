#include "reverb_mode_registry.h"
#include "room_reverb.h"
#include "hall_reverb.h"
#include "plate_reverb.h"
#include "spring_reverb.h"

namespace pedal {

static RoomReverb   s_room;
static HallReverb   s_hall;
static PlateReverb  s_plate;
static SpringReverb s_spring;

void ReverbModeRegistry::Init() {
    modes_[static_cast<uint8_t>(ReverbModeId::Room)]   = &s_room;
    modes_[static_cast<uint8_t>(ReverbModeId::Hall)]   = &s_hall;
    modes_[static_cast<uint8_t>(ReverbModeId::Plate)]  = &s_plate;
    modes_[static_cast<uint8_t>(ReverbModeId::Spring)] = &s_spring;

    for (int i = 0; i < NUM_REVERB_MODES; ++i) {
        modes_[i]->Init();
    }
}

ReverbMode* ReverbModeRegistry::get(ReverbModeId id) {
    return modes_[static_cast<uint8_t>(id)];
}

void ReverbModeRegistry::Reset(ReverbModeId id) {
    modes_[static_cast<uint8_t>(id)]->Reset();
}

} // namespace pedal
