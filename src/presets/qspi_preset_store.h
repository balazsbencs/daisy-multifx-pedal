#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"
#include <cstdint>
#include <cstring>

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
static_assert(sizeof(MultiPresetSlot) == 92,
    "MultiPresetSlot layout changed — update sysex encoding sizes if needed");

class QspiPresetStore {
public:
    void Init(daisy::QSPIHandle& qspi);

    int  GetActiveBank() const { return active_bank_; }
    int  GetActiveSlot() const { return active_slot_; }
    void SetActive(int bank, int slot);

    bool        LoadSlot(int bank, int slot, MultiPresetSlot& out) const;
    bool        SaveSlot(int bank, int slot, const MultiPresetSlot& data,
                         const char* name = nullptr);
    bool        LoadLiveState(MultiPresetSlot& out) const;
    bool        SaveLiveState(const MultiPresetSlot& data);
    const char* GetName(int bank, int slot) const;

private:
    static constexpr uint32_t kQspiBase     = 0x90000000u;
    static constexpr uint32_t kHeaderOffset = 0x007E0000u;
    static constexpr uint32_t kDataOffset   = 0x007E1000u; // 3 × 4 KB sectors
    static constexpr uint32_t kLiveOffset   = 0x007E4000u; // 1 × 4 KB sector
    static constexpr uint32_t kMagic        = 0x4D585053u; // "MXPS"
    static constexpr uint16_t kVersion      = 1u;
    static constexpr size_t   kSectorSize   = 4096u;
    static constexpr int      kSlotsPerSector = static_cast<int>(kSectorSize / sizeof(MultiPresetSlot)); // 44

    daisy::QSPIHandle* qspi_        = nullptr;
    bool               initialized_ = false;
    int                active_bank_ = 0;
    int                active_slot_ = 0;
    char               names_[PRESET_SLOT_COUNT][12]{};

    int  LinearIndex(int bank, int slot) const {
        return bank * PRESET_SLOTS_PER_BANK + slot;
    }
    bool ValidIndex(int bank, int slot) const {
        return bank >= 0 && bank < PRESET_BANK_COUNT &&
               slot >= 0 && slot < PRESET_SLOTS_PER_BANK;
    }
    void ReadHeader();
    void WriteHeader();
};

} // namespace pedal
