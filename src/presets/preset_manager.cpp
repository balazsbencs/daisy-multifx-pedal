#include "preset_manager.h"
#include <cstring>

namespace pedal {

static constexpr uint32_t kMagic   = 0x4D554C54; // "MULT"
static constexpr uint16_t kVersion = 1;

bool MultiPresetSlot::operator!=(const MultiPresetSlot& o) const {
    return memcmp(this, &o, sizeof(*this)) != 0;
}

bool MultiPresetManager::StorageState::operator!=(const StorageState& o) const {
    return memcmp(this, &o, sizeof(*this)) != 0;
}

void MultiPresetManager::Init(daisy::DaisySeed& hw) {
    StorageState defaults{};
    defaults.magic       = kMagic;
    defaults.version     = kVersion;
    defaults.active_slot = 0;
    for (int i = 0; i < PRESET_SLOT_COUNT; ++i) {
        defaults.slots[i].valid = 0;
    }

    new (storage_buf_) StorageType(hw.qspi);
    storage().Init(defaults, kPresetOffset);
    active_slot_ = static_cast<int>(storage().GetSettings().active_slot);
    initialized_ = true;
}

bool MultiPresetManager::LoadSlot(int slot, MultiPresetSlot& out) {
    if (!initialized_ || slot < 0 || slot >= PRESET_SLOT_COUNT) return false;
    const auto& s = storage().GetSettings().slots[slot];
    if (!s.valid) return false;
    out = s;
    return true;
}

bool MultiPresetManager::LoadActive(MultiPresetSlot& out) {
    return LoadSlot(active_slot_, out);
}

bool MultiPresetManager::SaveSlot(int slot, const MultiPresetSlot& data) {
    if (!initialized_ || slot < 0 || slot >= PRESET_SLOT_COUNT) return false;
    StorageState s = storage().GetSettings();
    s.slots[slot]  = data;
    s.slots[slot].valid = 1;
    s.active_slot  = static_cast<uint16_t>(active_slot_);
    storage().GetSettings() = s;
    storage().Save();
    return true;
}

} // namespace pedal
