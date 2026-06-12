#include "browse_machine.h"

namespace pedal {

static constexpr uint32_t kEntryHoldMs  = kPresetEntryHoldMs;
static constexpr uint32_t kInactivityMs = kPresetInactivityMs;

BrowseOutput BrowseMachine::Update(const BrowseInput& in) {
    BrowseOutput out{};
    out.in_browse = in_browse_;
    out.saved_ms  = browse_saved_ms_;

    if (!in_browse_) {
        // ── Normal mode: tap hold detection ───────────────────────────────────
        if (in.tap_pressed) {
            tap_hold_start_ = in.now_ms;
            tap_pending_    = false;
        }
        // Arm once hold threshold is reached; act on release.
        if (in.tap_held && !tap_pending_ &&
                (in.now_ms - tap_hold_start_) >= kEntryHoldMs) {
            tap_pending_ = true;
        }
        if (in.tap_released) {
            if (tap_pending_) {
                in_browse_      = true;
                browse_bank_    = in.current_bank;
                browse_slot_    = in.current_slot;
                last_browse_ms_ = in.now_ms;
                tap_pending_    = false;
                out.in_browse   = true;
                out.entered     = true;
                out.bank        = browse_bank_;
                out.slot        = browse_slot_;
            } else {
                // Short press — caller uses for tempo sync.
                out.short_tap = true;
            }
        }
    } else {
        // ── Browse mode ────────────────────────────────────────────────────────
        if (in.tap_pressed) {
            in_browse_     = false;
            out.in_browse  = false;
            out.exited     = true;
            out.set_active = true;
            out.bank       = browse_bank_;
            out.slot       = browse_slot_;
        } else if (in.fx0_pressed) {
            if (--browse_slot_ < 0) {
                browse_slot_ = PRESET_SLOTS_PER_BANK - 1;
                browse_bank_ = (browse_bank_ - 1 + PRESET_BANK_COUNT) % PRESET_BANK_COUNT;
            }
            last_browse_ms_ = in.now_ms;
            out.load_preset = true;
            out.hw_dirty    = true;
            out.bank        = browse_bank_;
            out.slot        = browse_slot_;
        } else if (in.fx2_pressed) {
            if (++browse_slot_ >= PRESET_SLOTS_PER_BANK) {
                browse_slot_ = 0;
                browse_bank_ = (browse_bank_ + 1) % PRESET_BANK_COUNT;
            }
            last_browse_ms_ = in.now_ms;
            out.load_preset = true;
            out.hw_dirty    = true;
            out.bank        = browse_bank_;
            out.slot        = browse_slot_;
        } else if (in.fx1_pressed) {
            last_browse_ms_  = in.now_ms;
            browse_saved_ms_ = in.now_ms;
            out.save_preset  = true;
            out.hw_dirty     = true;
            out.bank         = browse_bank_;
            out.slot         = browse_slot_;
            out.saved_ms     = browse_saved_ms_;
        } else if ((in.now_ms - last_browse_ms_) >= kInactivityMs) {
            in_browse_      = false;
            out.in_browse   = false;
            out.exited      = true;
            out.load_preset = true;
            out.bank        = in.active_bank;
            out.slot        = in.active_slot;
        }
    }
    return out;
}

} // namespace pedal
