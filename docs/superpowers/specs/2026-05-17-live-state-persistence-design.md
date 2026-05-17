# Live State Persistence — Design Spec

**Date:** 2026-05-17  
**Status:** Approved

## Goal

Persist the full pedal state (bypass on/off, mode selection, parameter values) to QSPI flash so the pedal restores its last configuration on every power-on.

## Scope

- Bypass state (`fx_enabled[3]`)
- Mode selection (`cur_mod`, `cur_delay`, `cur_reverb`)
- Normalized parameter values (`mod_norm[]`, `delay_norm[]`, `reverb_norm[]`)

Not in scope: tap tempo state, MIDI clock state, hold state.

## Data Model

### `MultiPresetSlot` (extended)

Add one field:

```cpp
uint8_t fx_enabled[3];  // index 0=mod, 1=delay, 2=reverb
```

This makes preset slots complete pedal snapshots (bypass + mode + params). Existing fields are unchanged.

### `StorageState` (extended)

Add one field alongside the existing `slots[PRESET_SLOT_COUNT]` array:

```cpp
MultiPresetSlot live_state;  // auto-saved live state
```

### Version bump

`kVersion` increments from `1` to `2`. On first boot with new firmware the stale flash version triggers `PersistentStorage` defaults: all slots empty (`valid = 0`), `live_state.valid = 0`. The hardcoded startup defaults apply as before — no data corruption.

## `MultiPresetManager` API additions

```cpp
bool LoadLiveState(MultiPresetSlot& out);   // returns false if valid == 0
bool SaveLiveState(const MultiPresetSlot& data);
```

Implementation mirrors `LoadSlot` / `SaveSlot` but targets `StorageState::live_state` directly.

## Main Loop — Debounced Writes

Two new variables in `main.cpp`:

```cpp
static bool     live_state_dirty = false;
static uint32_t last_change_ms   = 0;
```

Every change site (footswitch toggle, mode encoder rotation, param encoder turn, MIDI CC receive) sets:

```cpp
live_state_dirty = true;
last_change_ms   = now;
```

A new helper packs current state:

```cpp
static MultiPresetSlot SnapshotLiveState() {
    MultiPresetSlot s{};
    s.valid       = 1;
    s.mod_mode    = static_cast<uint8_t>(cur_mod);
    s.delay_mode  = static_cast<uint8_t>(cur_delay);
    s.reverb_mode = static_cast<uint8_t>(cur_reverb);
    memcpy(s.mod_norm,    mod_norm,    sizeof(mod_norm));
    memcpy(s.delay_norm,  delay_norm,  sizeof(delay_norm));
    memcpy(s.reverb_norm, reverb_norm, sizeof(reverb_norm));
    for (int i = 0; i < 3; ++i) s.fx_enabled[i] = fx_enabled[i] ? 1 : 0;
    return s;
}
```

Debounce check at the bottom of the main loop (after `audio_engine.SetParams`):

```cpp
constexpr uint32_t kLiveStateSaveDebounceMs = 2000;

if (live_state_dirty && (now - last_change_ms) >= kLiveStateSaveDebounceMs) {
    preset_manager.SaveLiveState(SnapshotLiveState());
    live_state_dirty = false;
}
```

## Boot Restore

After `preset_manager.Init(hw)`, before `hw.StartAudio(...)`:

```cpp
MultiPresetSlot live;
if (preset_manager.LoadLiveState(live)) {
    SwitchModMode(static_cast<ModModeId>(live.mod_mode));
    SwitchDelayMode(static_cast<DelayModeId>(live.delay_mode));
    SwitchReverbMode(static_cast<ReverbModeId>(live.reverb_mode));
    memcpy(mod_norm,    live.mod_norm,    sizeof(mod_norm));
    memcpy(delay_norm,  live.delay_norm,  sizeof(delay_norm));
    memcpy(reverb_norm, live.reverb_norm, sizeof(reverb_norm));
    for (int i = 0; i < 3; ++i) {
        fx_enabled[i] = live.fx_enabled[i];
        led_fx[i].Write(fx_enabled[i]);
    }
}
```

`led_fx` must already be initialised before this block (it is — GPIO init happens before `preset_manager.Init`).

## Flash Wear

`PersistentStorage::Save()` writes one QSPI sector (~100 k write cycles). With a 2-second debounce and typical live use (~100 state changes per session), flash endurance exceeds 1 000 sessions before first sector wear-out. Acceptable.

## Files Changed

| File | Change |
|------|--------|
| `src/presets/preset_manager.h` | Add `fx_enabled[3]` to `MultiPresetSlot`; add `live_state` to `StorageState`; declare `LoadLiveState` / `SaveLiveState` |
| `src/presets/preset_manager.cpp` | Bump `kVersion` to 2; implement `LoadLiveState` / `SaveLiveState` |
| `src/main.cpp` | Add dirty flag + debounce logic; add `SnapshotLiveState()`; restore live state on boot |
