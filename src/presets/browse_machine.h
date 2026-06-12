#pragma once
#include <cstdint>
#include "../config/constants.h"

namespace pedal {

// Input snapshot for one main-loop iteration.
struct BrowseInput {
    bool     tap_pressed;   // falling edge: button just pressed
    bool     tap_held;      // level: button is currently held
    uint32_t tap_held_ms;   // milliseconds since press started
    bool     tap_released;  // rising edge: button just released
    bool     fx0_pressed;   // MOD footswitch  → prev slot
    bool     fx1_pressed;   // DELAY footswitch → save slot
    bool     fx2_pressed;   // REVERB footswitch → next slot
    uint32_t now_ms;
    int      current_bank;  // committed preset_bank (for browse init on entry)
    int      current_slot;
    int      active_bank;   // QSPI-persisted active bank (for auto-exit restore)
    int      active_slot;
};

// What the machine wants the main loop to do after this update.
struct BrowseOutput {
    bool     in_browse;     // state after this update
    bool     entered;       // just entered browse mode
    bool     exited;        // just exited browse mode (tap confirm or inactivity)
    bool     load_preset;   // call LoadPreset(bank, slot)
    bool     save_preset;   // call SaveSlot(bank, slot, ...)
    bool     set_active;    // schedule deferred SetActive(bank, slot)
    bool     hw_dirty;      // set hw_live_state_dirty
    bool     short_tap;     // short press (no hold) — use for tempo sync
    int      bank;
    int      slot;
    uint32_t saved_ms;      // non-zero: show "SAVED" indicator until this timestamp
};

class BrowseMachine {
public:
    BrowseOutput Update(const BrowseInput& in);

    bool     InBrowse()    const { return in_browse_; }
    bool     TapPending()  const { return tap_pending_; }
    int      BrowseBank()  const { return browse_bank_; }
    int      BrowseSlot()  const { return browse_slot_; }
    uint32_t SavedMs()     const { return browse_saved_ms_; }

private:
    bool     in_browse_       = false;
    int      browse_bank_     = 0;
    int      browse_slot_     = 0;
    uint32_t last_browse_ms_  = 0;
    uint32_t browse_saved_ms_ = 0;
    bool     tap_pending_     = false;
    uint32_t tap_hold_start_  = 0;
};

} // namespace pedal
