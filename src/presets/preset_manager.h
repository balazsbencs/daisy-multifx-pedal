#pragma once
#include "daisy_seed.h"
#include "util/PersistentStorage.h"
#include "../config/constants.h"
#include "../config/delay_mode_id.h"
#include "../config/mod_mode_id.h"
#include "../config/reverb_mode_id.h"
#include <cstdint>

namespace pedal {

struct MultiPresetSlot {
    uint8_t valid;
    uint8_t mod_mode;
    uint8_t delay_mode;
    uint8_t reverb_mode;
    float   mod_norm[NUM_PARAMS];
    float   delay_norm[NUM_PARAMS];
    float   reverb_norm[NUM_PARAMS];
    uint8_t fx_enabled[3];

    bool operator!=(const MultiPresetSlot& o) const;
};

class MultiPresetManager {
public:
    void Init(daisy::DaisySeed& hw);

    int  GetActiveSlot() const { return active_slot_; }
    void SetActiveSlot(int slot) { active_slot_ = slot; }

    bool LoadActive(MultiPresetSlot& out);
    bool LoadSlot(int slot, MultiPresetSlot& out);
    bool SaveSlot(int slot, const MultiPresetSlot& data);
    bool LoadLiveState(MultiPresetSlot& out);
    bool SaveLiveState(const MultiPresetSlot& data);

private:
    struct StorageState {
        uint32_t       magic;
        uint16_t       version;
        uint16_t       active_slot;
        uint32_t       crc;
        MultiPresetSlot slots[PRESET_SLOT_COUNT];
        MultiPresetSlot live_state;

        bool operator!=(const StorageState& o) const;
    };

    using StorageType = daisy::PersistentStorage<StorageState>;
    alignas(StorageType) uint8_t storage_buf_[sizeof(StorageType)]{};

    StorageType& storage() {
        return *reinterpret_cast<StorageType*>(storage_buf_);
    }

    bool initialized_ = false;
    int  active_slot_ = 0;

    // QSPI offset: code starts at 0x00040000; presets at 0x007F0000 (end of window)
    static constexpr uint32_t kPresetOffset = 0x007F0000;
};

} // namespace pedal
