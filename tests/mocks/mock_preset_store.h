#pragma once
#include "presets/qspi_preset_store.h"
#include <array>
#include <cstring>

namespace pedal {

// In-memory drop-in for QspiPresetStore — same public contract, no QSPI.
class MockPresetStore {
public:
    bool LoadSlot(int bank, int slot, MultiPresetSlot& out) const {
        if (!valid_idx(bank, slot)) return false;
        const auto& s = slots_[idx(bank, slot)];
        if (s.valid != 1u) return false;
        out = s;
        return true;
    }

    bool SaveSlot(int bank, int slot, const MultiPresetSlot& data,
                  const char* name = nullptr) {
        if (!valid_idx(bank, slot)) return false;
        slots_[idx(bank, slot)]       = data;
        slots_[idx(bank, slot)].valid = 1;
        if (name) {
            strncpy(names_[idx(bank, slot)], name, 11);
            names_[idx(bank, slot)][11] = '\0';
        }
        return true;
    }

    const char* GetName(int bank, int slot) const {
        if (!valid_idx(bank, slot)) return "";
        return names_[idx(bank, slot)];
    }

    int  GetActiveBank() const { return active_bank_; }
    int  GetActiveSlot() const { return active_slot_; }
    void SetActive(int bank, int slot) { active_bank_ = bank; active_slot_ = slot; }

private:
    static constexpr int kTotal = PRESET_BANK_COUNT * PRESET_SLOTS_PER_BANK;
    static int  idx(int bank, int slot) { return bank * PRESET_SLOTS_PER_BANK + slot; }
    static bool valid_idx(int bank, int slot) {
        return bank >= 0 && bank < PRESET_BANK_COUNT &&
               slot >= 0 && slot < PRESET_SLOTS_PER_BANK;
    }

    std::array<MultiPresetSlot, kTotal> slots_{};
    char names_[kTotal][12]{};
    int  active_bank_ = 0;
    int  active_slot_ = 0;
};

} // namespace pedal
