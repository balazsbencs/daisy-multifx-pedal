#include "mocks/mock_preset_store.h"
#include "test_framework.h"
#include <cstring>

using namespace pedal;

static MultiPresetSlot make_slot(uint8_t mod, uint8_t delay, uint8_t reverb) {
    MultiPresetSlot s{};
    s.valid      = 1;
    s.mod_mode   = mod;
    s.delay_mode = delay;
    s.reverb_mode = reverb;
    for (int i = 0; i < NUM_PARAMS; ++i) {
        s.mod_norm[i]   = 0.1f * (i + 1);
        s.delay_norm[i] = 0.2f * (i + 1);
        s.reverb_norm[i]= 0.3f * (i + 1);
    }
    s.fx_enabled[0] = 1; s.fx_enabled[1] = 0; s.fx_enabled[2] = 1;
    return s;
}

void test_empty_slot_returns_false(TestSuite& t) {
    MockPresetStore store;
    MultiPresetSlot out{};
    t.check("empty slot: LoadSlot returns false", !store.LoadSlot(0, 0, out));
}

void test_save_load_roundtrip(TestSuite& t) {
    MockPresetStore store;
    MultiPresetSlot src = make_slot(2, 3, 4);
    store.SaveSlot(1, 5, src, "Init");

    MultiPresetSlot out{};
    t.check("roundtrip: LoadSlot returns true", store.LoadSlot(1, 5, out));
    t.check("roundtrip: mod_mode",    out.mod_mode   == 2);
    t.check("roundtrip: delay_mode",  out.delay_mode == 3);
    t.check("roundtrip: reverb_mode", out.reverb_mode == 4);
    t.check("roundtrip: fx_enabled[0]", out.fx_enabled[0] == 1);
    t.check("roundtrip: fx_enabled[1]", out.fx_enabled[1] == 0);
    t.check("roundtrip: mod_norm[3]",   out.mod_norm[3] == src.mod_norm[3]);
    t.check("roundtrip: delay_norm[6]", out.delay_norm[6] == src.delay_norm[6]);
}

void test_name_stored_and_retrieved(TestSuite& t) {
    MockPresetStore store;
    store.SaveSlot(0, 3, make_slot(1, 1, 1), "My Patch");
    t.check("name: returned correctly",
            std::strcmp(store.GetName(0, 3), "My Patch") == 0);
}

void test_save_one_slot_does_not_corrupt_adjacent(TestSuite& t) {
    MockPresetStore store;
    MultiPresetSlot a = make_slot(1, 1, 1);
    MultiPresetSlot b = make_slot(5, 6, 7);
    store.SaveSlot(0, 0, a, "A");
    store.SaveSlot(0, 1, b, "B");

    MultiPresetSlot ra{}, rb{};
    store.LoadSlot(0, 0, ra);
    store.LoadSlot(0, 1, rb);
    t.check("adjacent: slot 0 mod_mode intact",  ra.mod_mode == 1);
    t.check("adjacent: slot 1 mod_mode intact",  rb.mod_mode == 5);
    t.check("adjacent: slot 0 name intact",
            std::strcmp(store.GetName(0, 0), "A") == 0);
    t.check("adjacent: slot 1 name intact",
            std::strcmp(store.GetName(0, 1), "B") == 0);
}

void test_resave_updates_slot(TestSuite& t) {
    MockPresetStore store;
    store.SaveSlot(2, 4, make_slot(1, 1, 1), "Old");
    store.SaveSlot(2, 4, make_slot(9, 8, 7), "New");

    MultiPresetSlot out{};
    store.LoadSlot(2, 4, out);
    t.check("resave: mod_mode updated",  out.mod_mode == 9);
    t.check("resave: name updated",
            std::strcmp(store.GetName(2, 4), "New") == 0);
}

void test_out_of_range_indices_rejected(TestSuite& t) {
    MockPresetStore store;
    MultiPresetSlot dummy = make_slot(1, 1, 1);
    t.check("oob: negative bank rejected",
            !store.SaveSlot(-1, 0, dummy));
    t.check("oob: bank too large rejected",
            !store.SaveSlot(PRESET_BANK_COUNT, 0, dummy));
    t.check("oob: negative slot rejected",
            !store.SaveSlot(0, -1, dummy));
    t.check("oob: slot too large rejected",
            !store.SaveSlot(0, PRESET_SLOTS_PER_BANK, dummy));
}

void test_active_bank_slot(TestSuite& t) {
    MockPresetStore store;
    t.check("active: default bank=0", store.GetActiveBank() == 0);
    t.check("active: default slot=0", store.GetActiveSlot() == 0);
    store.SetActive(3, 7);
    t.check("active: bank updated",   store.GetActiveBank() == 3);
    t.check("active: slot updated",   store.GetActiveSlot() == 7);
}

void test_name_truncated_to_11_chars(TestSuite& t) {
    MockPresetStore store;
    store.SaveSlot(0, 0, make_slot(1,1,1), "12345678901234");
    const char* name = store.GetName(0, 0);
    t.check("name: max 11 chars", std::strlen(name) <= 11);
}

void run_preset_store_tests(TestSuite& t) {
    t.section("MockPresetStore");
    test_empty_slot_returns_false(t);
    test_save_load_roundtrip(t);
    test_name_stored_and_retrieved(t);
    test_save_one_slot_does_not_corrupt_adjacent(t);
    test_resave_updates_slot(t);
    test_out_of_range_indices_rejected(t);
    test_active_bank_slot(t);
    test_name_truncated_to_11_chars(t);
}
