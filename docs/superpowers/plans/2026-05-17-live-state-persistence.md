# Live State Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist bypass on/off, mode selection, and parameter values to QSPI flash so the pedal restores its last state on every power-on.

**Architecture:** Extend `MultiPresetSlot` with a `fx_enabled[3]` field and add a `live_state` slot to `StorageState`. A dirty flag + 2-second debounce in the main loop triggers `SaveLiveState()` on any user interaction. On boot, `LoadLiveState()` is called after `preset_manager.Init()` to restore the full state before audio starts.

**Tech Stack:** C++17, libDaisy `PersistentStorage<T>` (QSPI-backed), Daisy Seed firmware (no automated test framework — verification is build + flash).

---

## File Map

| File | Change |
|------|--------|
| `src/presets/preset_manager.h` | Add `fx_enabled[3]` to `MultiPresetSlot`; add `live_state` to `StorageState`; declare two new methods |
| `src/presets/preset_manager.cpp` | Bump `kVersion` to 2; implement `LoadLiveState` / `SaveLiveState` |
| `src/main.cpp` | Add dirty flag, `SnapshotLiveState()`, dirty marking at all change sites, debounce write, boot restore |

---

### Task 1: Extend `MultiPresetSlot` + `StorageState`, declare new methods

**Files:**
- Modify: `src/presets/preset_manager.h`

- [ ] **Step 1: Add `fx_enabled[3]` to `MultiPresetSlot`**

  In `src/presets/preset_manager.h`, replace the `MultiPresetSlot` struct (lines 12–22) with:

  ```cpp
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
  ```

- [ ] **Step 2: Add `live_state` field to `StorageState`**

  In `src/presets/preset_manager.h`, replace the `StorageState` struct (lines 36–44) with:

  ```cpp
  struct StorageState {
      uint32_t        magic;
      uint16_t        version;
      uint16_t        active_slot;
      uint32_t        crc;
      MultiPresetSlot slots[PRESET_SLOT_COUNT];
      MultiPresetSlot live_state;

      bool operator!=(const StorageState& o) const;
  };
  ```

- [ ] **Step 3: Declare `LoadLiveState` and `SaveLiveState` in `MultiPresetManager`**

  In `src/presets/preset_manager.h`, add two declarations to the `public:` section of `MultiPresetManager` (after the existing `SaveSlot` declaration):

  ```cpp
  bool LoadLiveState(MultiPresetSlot& out);
  bool SaveLiveState(const MultiPresetSlot& data);
  ```

- [ ] **Step 4: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -20
  ```

  Expected: build succeeds, `build/multi-fx.bin` produced. No new warnings about `MultiPresetSlot`.

- [ ] **Step 5: Commit**

  ```bash
  git add src/presets/preset_manager.h
  git commit -m "feat: extend MultiPresetSlot with fx_enabled, add live_state to StorageState"
  ```

---

### Task 2: Bump version + implement `LoadLiveState` / `SaveLiveState`

**Files:**
- Modify: `src/presets/preset_manager.cpp`

- [ ] **Step 1: Bump `kVersion` to 2**

  In `src/presets/preset_manager.cpp`, change line 7:

  ```cpp
  static constexpr uint16_t kVersion = 2;
  ```

  This causes `PersistentStorage` to detect the stale version on first boot and fall back to defaults (all slots `valid = 0`, `live_state.valid = 0`), so the hardcoded startup defaults apply cleanly.

- [ ] **Step 2: Implement `LoadLiveState`**

  Add after the `LoadActive` implementation (after line 41 in the original):

  ```cpp
  bool MultiPresetManager::LoadLiveState(MultiPresetSlot& out) {
      if (!initialized_) return false;
      const auto& s = storage().GetSettings().live_state;
      if (!s.valid) return false;
      out = s;
      return true;
  }
  ```

- [ ] **Step 3: Implement `SaveLiveState`**

  Add after `LoadLiveState`:

  ```cpp
  bool MultiPresetManager::SaveLiveState(const MultiPresetSlot& data) {
      if (!initialized_) return false;
      StorageState s    = storage().GetSettings();
      s.live_state      = data;
      s.live_state.valid = 1;
      storage().GetSettings() = s;
      storage().Save();
      return true;
  }
  ```

- [ ] **Step 4: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -20
  ```

  Expected: build succeeds. No linker errors for the two new methods.

- [ ] **Step 5: Commit**

  ```bash
  git add src/presets/preset_manager.cpp
  git commit -m "feat: bump storage version, implement LoadLiveState/SaveLiveState"
  ```

---

### Task 3: Add dirty flag, constant, and `SnapshotLiveState()` to `main.cpp`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add dirty flag and debounce constant**

  In `src/main.cpp`, add these two lines directly after the `fx_enabled` declaration (after line 51):

  ```cpp
  static bool     live_state_dirty            = false;
  static uint32_t last_change_ms              = 0;
  constexpr uint32_t kLiveStateSaveDebounceMs = 2000;
  ```

- [ ] **Step 2: Add `SnapshotLiveState()` helper**

  Add this function after the `WrapIndex` helper (after line 66), before `ApplyEncoderEdit`:

  ```cpp
  static MultiPresetSlot SnapshotLiveState() {
      MultiPresetSlot s{};
      s.valid       = 1;
      s.mod_mode    = static_cast<uint8_t>(cur_mod);
      s.delay_mode  = static_cast<uint8_t>(cur_delay);
      s.reverb_mode = static_cast<uint8_t>(cur_reverb);
      for (int i = 0; i < NUM_PARAMS; ++i) {
          s.mod_norm[i]    = mod_norm[i];
          s.delay_norm[i]  = delay_norm[i];
          s.reverb_norm[i] = reverb_norm[i];
      }
      for (int i = 0; i < 3; ++i) s.fx_enabled[i] = fx_enabled[i] ? 1 : 0;
      return s;
  }
  ```

- [ ] **Step 3: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -20
  ```

  Expected: build succeeds. The helper compiles; `cur_mod`, `mod_norm`, etc. are all in scope.

- [ ] **Step 4: Commit**

  ```bash
  git add src/main.cpp
  git commit -m "feat: add live state dirty flag and SnapshotLiveState helper"
  ```

---

### Task 4: Mark dirty at all change sites + debounce write

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Mark dirty on footswitch toggle**

  Replace the footswitch block (lines 183–189):

  ```cpp
  // ── Effect on/off footswitches ───────────────────────────────────────
  for (int i = 0; i < 3; ++i) {
      if (ctrl.fx_pressed[i]) {
          fx_enabled[i]    = !fx_enabled[i];
          led_fx[i].Write(fx_enabled[i]);
          live_state_dirty = true;
          last_change_ms   = now;
      }
  }
  ```

- [ ] **Step 2: Mark dirty on mode encoder rotation**

  Replace the mode delta block (lines 209–233):

  ```cpp
  // ── Mode selection within active page (mode encoder rotation) ─────────
  const int mode_delta = ctrl.mode_encoder_increment;
  if (mode_delta) {
      const int dir = (mode_delta > 0) ? 1 : -1;
      switch (active_page) {
          case 0: {
              const int m = WrapIndex(static_cast<int>(cur_mod) + dir,
                                      NUM_MOD_MODES);
              SwitchModMode(static_cast<ModModeId>(m));
              break;
          }
          case 1: {
              const int m = WrapIndex(static_cast<int>(cur_delay) + dir,
                                      NUM_DELAY_MODES);
              SwitchDelayMode(static_cast<DelayModeId>(m));
              break;
          }
          case 2: {
              const int m = WrapIndex(static_cast<int>(cur_reverb) + dir,
                                      NUM_REVERB_MODES);
              SwitchReverbMode(static_cast<ReverbModeId>(m));
              break;
          }
      }
      live_state_dirty = true;
      last_change_ms   = now;
  }
  ```

- [ ] **Step 3: Mark dirty on param encoder turns**

  Replace the param encoder block (lines 235–251):

  ```cpp
  // ── Parameter encoders → active page params ────────────────────────────
  float* active_norm = (active_page == 0) ? mod_norm :
                       (active_page == 1) ? delay_norm : reverb_norm;

  const bool shift = ctrl.mode_encoder_held;
  for (int p = 0; p < 4; ++p) {
      const int delta = ctrl.param_encoder_increment[p];
      if (!delta) continue;
      if (shift) {
          mode_hold_consumed = true;
      }
      const int param_idx = shift ? (p + 4) : p;
      if (param_idx < NUM_PARAMS) {
          ApplyEncoderEdit(active_norm[param_idx], delta, now,
                           last_enc_tick_ms[param_idx]);
          live_state_dirty = true;
          last_change_ms   = now;
      }
  }
  ```

- [ ] **Step 4: Mark dirty on MIDI CC receive**

  Replace the MIDI CC block (lines 274–278):

  ```cpp
  for (int p = 0; p < NUM_PARAMS; ++p) {
      if (midi.mod_cc_rx[p])    { mod_norm[p]    = midi.mod_cc[p];    live_state_dirty = true; last_change_ms = now; }
      if (midi.delay_cc_rx[p])  { delay_norm[p]  = midi.delay_cc[p];  live_state_dirty = true; last_change_ms = now; }
      if (midi.reverb_cc_rx[p]) { reverb_norm[p] = midi.reverb_cc[p]; live_state_dirty = true; last_change_ms = now; }
  }
  ```

- [ ] **Step 5: Add debounce write at the bottom of the main loop**

  Add this block at the very end of `while (true)`, after the display update block (after line 307 `}`):

  ```cpp
  // ── Live state auto-save (debounced) ──────────────────────────────────
  if (live_state_dirty && (now - last_change_ms) >= kLiveStateSaveDebounceMs) {
      preset_manager.SaveLiveState(SnapshotLiveState());
      live_state_dirty = false;
  }
  ```

- [ ] **Step 6: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -20
  ```

  Expected: build succeeds, no new warnings.

- [ ] **Step 7: Commit**

  ```bash
  git add src/main.cpp
  git commit -m "feat: mark live state dirty at all change sites, debounce save"
  ```

---

### Task 5: Restore live state on boot

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add boot restore block**

  In `main()`, locate the line `preset_manager.Init(hw);` (currently line 167). Add the following block directly after it, before `hw.StartAudio(AudioEngine::AudioCallback);`:

  ```cpp
  {
      MultiPresetSlot live;
      if (preset_manager.LoadLiveState(live)) {
          SwitchModMode(static_cast<ModModeId>(live.mod_mode));
          SwitchDelayMode(static_cast<DelayModeId>(live.delay_mode));
          SwitchReverbMode(static_cast<ReverbModeId>(live.reverb_mode));
          for (int i = 0; i < NUM_PARAMS; ++i) {
              mod_norm[i]    = live.mod_norm[i];
              delay_norm[i]  = live.delay_norm[i];
              reverb_norm[i] = live.reverb_norm[i];
          }
          for (int i = 0; i < 3; ++i) {
              fx_enabled[i] = live.fx_enabled[i];
              led_fx[i].Write(fx_enabled[i]);
          }
      }
  }
  ```

  Note: `led_fx[]` GPIO is initialized at lines 152–155, `audio_engine.Init` at line 157, and the three registries at lines 148–150 — all before this block, so `SwitchXxxMode` and `led_fx[i].Write` are safe to call here.

- [ ] **Step 2: Build final binary**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -20
  ```

  Expected: build succeeds, `build/multi-fx.bin` produced.

- [ ] **Step 3: Commit**

  ```bash
  git add src/main.cpp
  git commit -m "feat: restore live state (bypass, modes, params) on boot"
  ```

---

### Task 6: Flash and verify on hardware

- [ ] **Step 1: Flash the firmware**

  Press RESET only (enters Daisy Bootloader DFU). Within 2 seconds run:

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make program-dfu
  ```

- [ ] **Step 2: First-boot sanity check**

  On first boot with new firmware the QSPI version mismatch resets storage. The pedal starts with hardcoded defaults: mod on, delay/reverb off, Chorus/Tape/Hall. Verify the LEDs match `{true, false, false}`.

- [ ] **Step 3: Verify live state saves and restores**

  1. Toggle all three effects on (all LEDs lit).
  2. Scroll the mod encoder to a different mode (e.g., Phaser).
  3. Wait 3 seconds (debounce fires, state is saved to QSPI).
  4. Power cycle the pedal.
  5. Expected: all three effects are on, mod stage shows Phaser.

- [ ] **Step 4: Verify debounce doesn't write on rapid changes**

  Rapidly toggle an effect on/off several times, then power cycle before 2 seconds elapse. The state saved should reflect the last stable state from the previous session, not the mid-toggle state — because the save hadn't fired yet.
