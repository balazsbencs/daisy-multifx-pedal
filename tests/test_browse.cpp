#include "presets/browse_machine.h"
#include "test_framework.h"
#include <cstdint>

using namespace pedal;

// ── Helpers ────────────────────────────────────────────────────────────────

static BrowseInput idle(uint32_t now, int bank = 0, int slot = 0) {
    BrowseInput in{};
    in.now_ms       = now;
    in.current_bank = bank;
    in.current_slot = slot;
    in.active_bank  = bank;
    in.active_slot  = slot;
    return in;
}

// Simulate holding the tap button past the entry threshold, then releasing.
static BrowseOutput do_hold_entry(BrowseMachine& m, uint32_t& now,
                                  int bank = 0, int slot = 0) {
    BrowseInput in = idle(now, bank, slot);
    in.tap_pressed = true;
    m.Update(in);
    now += kPresetEntryHoldMs + 1;
    in = idle(now, bank, slot);
    in.tap_held    = true;
    in.tap_held_ms = kPresetEntryHoldMs + 1;
    m.Update(in);
    in = idle(now, bank, slot);
    in.tap_released = true;
    return m.Update(in);
}

// ── Tests ──────────────────────────────────────────────────────────────────

void test_short_tap_fires_tempo_not_browse(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    BrowseInput in = idle(now);
    in.tap_pressed = true;
    m.Update(in);
    now += 50;
    in = idle(now);
    in.tap_released = true;
    in.tap_held_ms  = 50;
    auto out = m.Update(in);
    t.check("short tap: short_tap=true",  out.short_tap);
    t.check("short tap: not in browse",   !out.in_browse);
    t.check("short tap: not entered",     !out.entered);
}

void test_hold_tap_enters_browse_on_release(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    auto out = do_hold_entry(m, now, 2, 5);
    t.check("hold entry: entered=true",         out.entered);
    t.check("hold entry: in_browse=true",       out.in_browse);
    t.check("hold entry: bank mirrors current", out.bank == 2);
    t.check("hold entry: slot mirrors current", out.slot == 5);
    t.check("hold entry: InBrowse() after",     m.InBrowse());
}

void test_hold_below_threshold_does_not_enter(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    BrowseInput in = idle(now);
    in.tap_pressed = true;
    m.Update(in);
    now += kPresetEntryHoldMs - 10;
    in = idle(now);
    in.tap_released = true;
    in.tap_held_ms  = kPresetEntryHoldMs - 10;
    auto out = m.Update(in);
    t.check("under-threshold: not entered",  !out.entered);
    t.check("under-threshold: short_tap",    out.short_tap);
    t.check("under-threshold: not browsing", !m.InBrowse());
}

void test_tap_press_exits_browse(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now);
    now += 100;
    BrowseInput in = idle(now);
    in.tap_pressed = true;
    auto out = m.Update(in);
    t.check("exit: exited=true",      out.exited);
    t.check("exit: in_browse=false",  !out.in_browse);
    t.check("exit: set_active=true",  out.set_active);
    t.check("exit: InBrowse() false", !m.InBrowse());
}

void test_no_double_exit_on_entry_release(TestSuite& t) {
    // The release that triggers entry must NOT simultaneously exit browse.
    BrowseMachine m;
    uint32_t now = 0;
    auto entry_out = do_hold_entry(m, now);
    t.check("entry release: in_browse=true",  entry_out.in_browse);
    t.check("entry release: exited=false",    !entry_out.exited);
    t.check("entry release: InBrowse() true", m.InBrowse());
}

void test_next_slot(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 0, 3);
    now += 50;
    BrowseInput in = idle(now);
    in.fx2_pressed = true;
    auto out = m.Update(in);
    t.check("next: load_preset=true",  out.load_preset);
    t.check("next: slot advanced",     out.slot == 4);
    t.check("next: bank unchanged",    out.bank == 0);
    t.check("next: hw_dirty=true",     out.hw_dirty);
}

void test_prev_slot(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 0, 3);
    now += 50;
    BrowseInput in = idle(now);
    in.fx0_pressed = true;
    auto out = m.Update(in);
    t.check("prev: load_preset=true",  out.load_preset);
    t.check("prev: slot decremented",  out.slot == 2);
}

void test_next_wraps_to_next_bank(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 1, PRESET_SLOTS_PER_BANK - 1);
    now += 50;
    BrowseInput in = idle(now);
    in.fx2_pressed = true;
    auto out = m.Update(in);
    t.check("next wrap: slot=0",  out.slot == 0);
    t.check("next wrap: bank=2",  out.bank == 2);
}

void test_prev_wraps_to_prev_bank(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 2, 0);
    now += 50;
    BrowseInput in = idle(now);
    in.fx0_pressed = true;
    auto out = m.Update(in);
    t.check("prev wrap: slot=last",  out.slot == PRESET_SLOTS_PER_BANK - 1);
    t.check("prev wrap: bank=1",     out.bank == 1);
}

void test_prev_wraps_bank_0(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 0, 0);
    now += 50;
    BrowseInput in = idle(now);
    in.fx0_pressed = true;
    auto out = m.Update(in);
    t.check("bank 0 wrap: bank=last", out.bank == PRESET_BANK_COUNT - 1);
    t.check("bank 0 wrap: slot=last", out.slot  == PRESET_SLOTS_PER_BANK - 1);
}

void test_save_preset(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 0, 2);
    now += 50;
    BrowseInput in = idle(now);
    in.fx1_pressed = true;
    auto out = m.Update(in);
    t.check("save: save_preset=true", out.save_preset);
    t.check("save: bank=0",           out.bank == 0);
    t.check("save: slot=2",           out.slot == 2);
    t.check("save: hw_dirty=true",    out.hw_dirty);
    t.check("save: saved_ms set",     out.saved_ms != 0);
    t.check("save: still browsing",   out.in_browse);
}

void test_inactivity_auto_exit(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now, 0, 0);
    now += kPresetInactivityMs + 1;
    BrowseInput in = idle(now);
    in.active_bank = 3;
    in.active_slot = 7;
    auto out = m.Update(in);
    t.check("inactivity: exited=true",      out.exited);
    t.check("inactivity: in_browse=false",  !out.in_browse);
    t.check("inactivity: load active bank", out.bank == 3);
    t.check("inactivity: load active slot", out.slot == 7);
    t.check("inactivity: load_preset=true", out.load_preset);
}

void test_navigation_resets_inactivity_timer(TestSuite& t) {
    BrowseMachine m;
    uint32_t now = 0;
    do_hold_entry(m, now);
    // Navigate just before timeout.
    now += kPresetInactivityMs - 50;
    BrowseInput in = idle(now);
    in.fx2_pressed = true;
    m.Update(in);
    // Another kPresetInactivityMs - 1 ms: must NOT auto-exit yet.
    now += kPresetInactivityMs - 1;
    auto out = m.Update(idle(now));
    t.check("nav resets timer: still browsing", out.in_browse);
    t.check("nav resets timer: not exited",     !out.exited);
}

void run_browse_tests(TestSuite& t) {
    t.section("BrowseMachine");
    test_short_tap_fires_tempo_not_browse(t);
    test_hold_tap_enters_browse_on_release(t);
    test_hold_below_threshold_does_not_enter(t);
    test_tap_press_exits_browse(t);
    test_no_double_exit_on_entry_release(t);
    test_next_slot(t);
    test_prev_slot(t);
    test_next_wraps_to_next_bank(t);
    test_prev_wraps_to_prev_bank(t);
    test_prev_wraps_bank_0(t);
    test_save_preset(t);
    test_inactivity_auto_exit(t);
    test_navigation_resets_inactivity_timer(t);
}
