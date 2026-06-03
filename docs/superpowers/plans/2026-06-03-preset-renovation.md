# Preset System Renovation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all broken preset save/load flows and add real-time bidirectional sync between hardware and editor.

**Architecture:** New `LIVE_STATE` SysEx message (cmd `0x82`) carries the full hardware state to the editor; firmware broadcasts it on every hardware-control change (debounced 100 ms). The Tauri backend replaces the broken `GET_ALL` blast with a paced sequential sync (25 ms between requests). The editor applies hardware state updates without echoing CC back, and correctly updates its local cache after every save or import.

**Tech Stack:** C++ (Daisy firmware, no tests), Rust + Tauri (backend, unit tests in `sysex.rs`), TypeScript + React (editor, manual verification).

**Spec:** `docs/superpowers/specs/2026-06-03-preset-renovation-design.md`

---

## File Map

| File | Change |
|------|--------|
| `src/midi/midi_handler.h` | Add `request_live_state` to `MultiMidiState`; add `SendLiveState` declaration |
| `src/midi/midi_handler.cpp` | Handle cmd `0x0B`; implement `SendLiveState` |
| `src/main.cpp` | Add `hw_live_state_dirty` broadcast logic; browse-mode save fixes; display event |
| `editor/src-tauri/src/sysex.rs` | Add `build_get_live_state`, `parse_live_state`; add unit tests |
| `editor/src-tauri/src/commands.rs` | Replace `get_all_presets` with `sync_all_presets`; add `get_live_state` |
| `editor/src-tauri/src/midi.rs` | Auto-send `GET_LIVE_STATE` after connect |
| `editor/src-tauri/src/main.rs` | Register new commands; remove old |
| `editor/src/lib/presetCodec.ts` | Add `valid: number` to `ParsedPreset`; return `raw[0]` |
| `editor/src/hooks/useMidi.ts` | Handle `0x82`; fix `SavePending`; sync progress; expose `liveState`, `setPresets`, `syncAllPresets` |
| `editor/src/App.tsx` | Apply `liveState` effect; fix `handleImportDone`; fix `handlePresetSelect`; fix `isDirty`; trigger sync on connect |
| `editor/src/components/PresetHeader.tsx` | Add `syncProgress` prop and progress bar |

---

## Task 1: Firmware — MIDI handler protocol additions

**Files:**
- Modify: `src/midi/midi_handler.h`
- Modify: `src/midi/midi_handler.cpp`

- [ ] **Step 1: Add `request_live_state` to `MultiMidiState` in `midi_handler.h`**

  Add one field to the struct (after `fx_enable_val`):
  ```cpp
  bool fx_enable_val    = false;

  bool request_live_state = false; // GET_LIVE_STATE (0x0B) received
  ```

- [ ] **Step 2: Add `SendLiveState` declaration to `MidiHandlerPedal` in `midi_handler.h`**

  Add after the existing `SendAck` private declaration:
  ```cpp
  void SendAck(uint8_t cmd, bool ok);
  void SendLiveState(int bank, int slot, const MultiPresetSlot& state);
  ```

- [ ] **Step 3: Handle cmd `0x0B` in `HandleSysEx` in `midi_handler.cpp`**

  Add a new case before `default:` in the switch:
  ```cpp
  case 0x0Bu: { // GET_LIVE_STATE
      out.request_live_state = true;
      SendAck(cmd, true);
      break;
  }
  ```

- [ ] **Step 4: Implement `SendLiveState` in `midi_handler.cpp`**

  Add after the `SendAck` definition:
  ```cpp
  void MidiHandlerPedal::SendLiveState(int bank, int slot,
                                        const MultiPresetSlot& state) {
      if (!store_) return;
      constexpr size_t kEncLen = EncodedSize(92);
      // Frame: F0 7D 82 bank slot encoded[106] F7  (total 112 bytes)
      static uint8_t buf[1u + 1u + 1u + 1u + 1u + kEncLen + 1u];
      size_t i = 0;
      buf[i++] = 0xF0u;
      buf[i++] = 0x7Du;
      buf[i++] = 0x82u;
      buf[i++] = static_cast<uint8_t>(bank);
      buf[i++] = static_cast<uint8_t>(slot);
      uint8_t raw[92];
      memcpy(raw, &state, sizeof(state));
      i += Encode7bit(raw, sizeof(raw), buf + i);
      buf[i++] = 0xF7u;
      usb_.SendMessage(buf, i);
  }
  ```

- [ ] **Step 5: Build firmware to verify no compile errors**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -5
  ```
  Expected: `Linking build/multi-fx.elf` with no errors.

- [ ] **Step 6: Commit**

  ```bash
  git add src/midi/midi_handler.h src/midi/midi_handler.cpp
  git commit -m "feat(firmware): add GET_LIVE_STATE (0x0B) and SendLiveState (0x82)"
  ```

---

## Task 2: Firmware — Hardware broadcast in `main.cpp`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add hardware-broadcast dirty flag and debounce constant**

  In `src/main.cpp`, find the block of statics that includes `live_state_dirty` (around line 54–56). Add below them:
  ```cpp
  static bool     hw_live_state_dirty        = false;
  static uint32_t hw_last_hw_change_ms       = 0u;
  constexpr uint32_t kLiveBroadcastDebounceMs = 100u;
  ```

- [ ] **Step 2: Set flag on FX footswitch toggles (Normal mode)**

  Find the `if (ctrl.fx_pressed[i])` block inside `if (preset_mode == PresetMode::Normal)` (around line 280). Add the two new lines:
  ```cpp
  if (ctrl.fx_pressed[i]) {
      fx_enabled[i]    = !fx_enabled[i];
      led_fx[i].Write(fx_enabled[i]);
      live_state_dirty     = true;
      last_change_ms       = now;
      hw_live_state_dirty  = true;   // add
      hw_last_hw_change_ms = now;    // add
  }
  ```

- [ ] **Step 3: Set flag on mode encoder rotation**

  Find the three `SwitchModMode` / `SwitchDelayMode` / `SwitchReverbMode` case blocks (around lines 346–364). Each case already ends with `break;`. After the switch, the code sets `live_state_dirty = true; last_change_ms = now;`. Add the new lines immediately after:
  ```cpp
  live_state_dirty     = true;
  last_change_ms       = now;
  hw_live_state_dirty  = true;   // add
  hw_last_hw_change_ms = now;    // add
  ```

- [ ] **Step 4: Set flag on parameter encoder turns**

  Find the `if (param_idx < NUM_PARAMS)` block (around line 381). After the existing dirty lines:
  ```cpp
  live_state_dirty     = true;
  last_change_ms       = now;
  hw_live_state_dirty  = true;   // add
  hw_last_hw_change_ms = now;    // add
  ```

- [ ] **Step 5: Set flag on browse-mode preset navigation loads**

  Find `LoadPreset(browse_bank, browse_slot)` calls inside the `ctrl.fx_pressed[0]` and `ctrl.fx_pressed[2]` branches in browse mode (around lines 294, 302). After each `last_browse_ms = now;` line:
  ```cpp
  // after ctrl.fx_pressed[0] block:
  LoadPreset(browse_bank, browse_slot);
  last_browse_ms       = now;
  hw_live_state_dirty  = true;   // add
  hw_last_hw_change_ms = now;    // add

  // after ctrl.fx_pressed[2] block (same pattern):
  LoadPreset(browse_bank, browse_slot);
  last_browse_ms       = now;
  hw_live_state_dirty  = true;   // add
  hw_last_hw_change_ms = now;    // add
  ```

- [ ] **Step 6: Handle `midi.request_live_state` after the MIDI poll**

  Find the `if (midi.fx_enable_change)` block (around line 458). After its closing brace, add:
  ```cpp
  if (midi.request_live_state) {
      midi_handler.SendLiveState(preset_bank, preset_slot, SnapshotLiveState());
  }
  ```

- [ ] **Step 7: Add broadcast loop after the existing auto-save block**

  Find the live-state auto-save block at the bottom of the loop (around line 500):
  ```cpp
  if (live_state_dirty && (now - last_change_ms) >= kLiveStateSaveDebounceMs) {
      preset_store.SaveLiveState(SnapshotLiveState());
      live_state_dirty = false;
  }
  ```
  Add immediately after:
  ```cpp
  if (hw_live_state_dirty && (now - hw_last_hw_change_ms) >= kLiveBroadcastDebounceMs) {
      midi_handler.SendLiveState(preset_bank, preset_slot, SnapshotLiveState());
      hw_live_state_dirty = false;
  }
  ```

- [ ] **Step 8: Build firmware**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -5
  ```
  Expected: no errors.

- [ ] **Step 9: Commit**

  ```bash
  git add src/main.cpp
  git commit -m "feat(firmware): broadcast LIVE_STATE on hardware control changes"
  ```

---

## Task 3: Firmware — Browse-mode save fixes + display feedback

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add `browse_saved_ms` static and `kSavedFlashMs` constant**

  Near the other browse-mode statics (around line 63–68) add:
  ```cpp
  static constexpr uint32_t kSavedFlashMs   = 1000u;
  static uint32_t           browse_saved_ms = 0u;
  ```

- [ ] **Step 2: Fix `SaveSlot` call to pass a name and set the flash timer**

  Find the `ctrl.fx_pressed[1]` block in browse mode (around line 305). Replace:
  ```cpp
  if (ctrl.fx_pressed[1]) {
      const MultiPresetSlot snap = SnapshotLiveState();
      preset_store.SaveSlot(browse_bank, browse_slot, snap);
      last_browse_ms = now;
  }
  ```
  With:
  ```cpp
  if (ctrl.fx_pressed[1]) {
      const MultiPresetSlot snap = SnapshotLiveState();
      const char* existing_name  = preset_store.GetName(browse_bank, browse_slot);
      const char* save_name      = (existing_name && existing_name[0] != '\0')
                                   ? existing_name : "Preset";
      preset_store.SaveSlot(browse_bank, browse_slot, snap, save_name);
      last_browse_ms       = now;
      browse_saved_ms      = now;
      hw_live_state_dirty  = true;
      hw_last_hw_change_ms = now;
  }
  ```

- [ ] **Step 3: Thread `PresetUiEvent` through the display call**

  Find the `display.Update(...)` call (around line 492). Replace:
  ```cpp
  display.Update(active_page, shift,
                 cur_mod, cur_delay, cur_reverb,
                 buf.mod, buf.delay, buf.reverb,
                 fx_enabled, hold_active,
                 preset_bank * PRESET_SLOTS_PER_BANK + preset_slot,
                 PresetUiEvent::None, now);
  ```
  With:
  ```cpp
  const PresetUiEvent ui_event =
      (browse_saved_ms != 0u && (now - browse_saved_ms) < kSavedFlashMs)
          ? PresetUiEvent::Saved
          : PresetUiEvent::None;
  display.Update(active_page, shift,
                 cur_mod, cur_delay, cur_reverb,
                 buf.mod, buf.delay, buf.reverb,
                 fx_enabled, hold_active,
                 preset_bank * PRESET_SLOTS_PER_BANK + preset_slot,
                 ui_event, now);
  ```

- [ ] **Step 4: Build firmware**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | tail -5
  ```
  Expected: no errors.

- [ ] **Step 5: Flash to hardware and verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make program-dfu
  ```
  Manual check: enter browse mode (hold TAP 1 s → LEDs blink), navigate to any slot, press DELAY footswitch. Display should show "SAVED" for 1 s then return to normal.

- [ ] **Step 6: Commit**

  ```bash
  git add src/main.cpp
  git commit -m "fix(firmware): browse-mode save persists name and flashes SAVED on display"
  ```

---

## Task 4: Rust — SysEx codec additions

**Files:**
- Modify: `editor/src-tauri/src/sysex.rs`

- [ ] **Step 1: Add `build_get_live_state` function**

  In `editor/src-tauri/src/sysex.rs`, add after `build_get_all`:
  ```rust
  pub fn build_get_live_state() -> Vec<u8> {
      vec![0xF0, 0x7D, 0x0B, 0xF7]
  }
  ```

- [ ] **Step 2: Add `parse_live_state` function**

  Add after `parse_preset_data`:
  ```rust
  /// Parse a LIVE_STATE response (cmd 0x82).
  /// Returns (bank, slot, raw_92_bytes) or None if malformed.
  pub fn parse_live_state(frame: &[u8]) -> Option<(u8, u8, Vec<u8>)> {
      // frame: F0 7D 82 bank slot encoded[106] F7
      if frame.len() < 5 || frame[0] != 0xF0 || frame[1] != 0x7D || frame[2] != 0x82 {
          return None;
      }
      let bank = frame[3];
      let slot = frame[4];
      if frame.len() < 5 + 106 + 1 { return None; }
      let raw = decode_7bit(&frame[5..frame.len().saturating_sub(1)]);
      if raw.len() != 92 { return None; }
      Some((bank, slot, raw))
  }
  ```

- [ ] **Step 3: Write unit test for `build_get_live_state`**

  Add to the existing `#[cfg(test)] mod tests` block:
  ```rust
  #[test]
  fn get_live_state_frame() {
      let frame = build_get_live_state();
      assert_eq!(frame, vec![0xF0, 0x7D, 0x0B, 0xF7]);
  }
  ```

- [ ] **Step 4: Write unit test for `parse_live_state` round-trip**

  Add to the same `tests` block:
  ```rust
  #[test]
  fn live_state_round_trip() {
      let raw: Vec<u8> = (0u8..92).collect();
      let encoded = encode_7bit(&raw);
      // Build a synthetic LIVE_STATE frame: F0 7D 82 bank=3 slot=7 encoded F7
      let mut frame = vec![0xF0u8, 0x7D, 0x82, 3, 7];
      frame.extend_from_slice(&encoded);
      frame.push(0xF7);

      let result = parse_live_state(&frame);
      assert!(result.is_some());
      let (bank, slot, decoded) = result.unwrap();
      assert_eq!(bank, 3);
      assert_eq!(slot, 7);
      assert_eq!(decoded, raw);
  }

  #[test]
  fn parse_live_state_rejects_wrong_cmd() {
      // cmd 0x81 (PRESET_DATA) should be rejected by parse_live_state
      let raw: Vec<u8> = vec![0u8; 92];
      let encoded = encode_7bit(&raw);
      let mut frame = vec![0xF0u8, 0x7D, 0x81, 0, 0];
      frame.extend_from_slice(&encoded);
      frame.push(0xF7);
      assert!(parse_live_state(&frame).is_none());
  }
  ```

- [ ] **Step 5: Run tests**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx/editor/src-tauri && cargo test 2>&1 | tail -20
  ```
  Expected:
  ```
  test tests::get_live_state_frame ... ok
  test tests::live_state_round_trip ... ok
  test tests::parse_live_state_rejects_wrong_cmd ... ok
  test tests::round_trip_92_bytes ... ok
  test tests::all_high_bits ... ok
  ```

- [ ] **Step 6: Commit**

  ```bash
  git add editor/src-tauri/src/sysex.rs
  git commit -m "feat(tauri): add build_get_live_state and parse_live_state with tests"
  ```

---

## Task 5: Rust — New commands and sequential sync

**Files:**
- Modify: `editor/src-tauri/src/commands.rs`

- [ ] **Step 1: Add `use tauri::AppHandle` import**

  `commands.rs` currently imports:
  ```rust
  use std::sync::{Arc, Mutex};
  use tauri::State;
  use crate::midi::{self, MidiState};
  use crate::sysex;
  ```
  Add `AppHandle` to the Tauri import:
  ```rust
  use tauri::{AppHandle, State};
  ```

- [ ] **Step 2: Replace `get_all_presets` with `sync_all_presets`**

  Remove:
  ```rust
  #[tauri::command]
  pub fn get_all_presets(state: State<SharedMidi>) -> Result<(), String> {
      midi::send_raw(&state, &sysex::build_get_all())
  }
  ```
  Add:
  ```rust
  /// Fetches all 100 presets sequentially with 25 ms pacing to avoid
  /// overflowing the firmware's USB TX FIFO (firmware GET_ALL sends 100×~124 bytes at once).
  #[tauri::command]
  pub fn sync_all_presets(
      state: State<SharedMidi>,
      app: AppHandle,
  ) -> Result<(), String> {
      let state = Arc::clone(&state);
      std::thread::spawn(move || {
          for bank in 0u8..10 {
              for slot in 0u8..10 {
                  let progress = bank * 10 + slot + 1;
                  if midi::send_raw(&state, &sysex::build_get_preset(bank, slot)).is_err() {
                      break;
                  }
                  let _ = app.emit("midi-sync-progress", progress as u32);
                  std::thread::sleep(std::time::Duration::from_millis(25));
              }
          }
          let _ = app.emit("midi-sync-done", ());
      });
      Ok(())
  }
  ```

- [ ] **Step 3: Add `get_live_state` command**

  Add after `sync_all_presets`:
  ```rust
  #[tauri::command]
  pub fn get_live_state(state: State<SharedMidi>) -> Result<(), String> {
      midi::send_raw(&state, &sysex::build_get_live_state())
  }
  ```

- [ ] **Step 4: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx/editor/src-tauri && cargo build 2>&1 | grep -E "^error" | head -10
  ```
  Expected: no output (no errors).

- [ ] **Step 5: Commit**

  ```bash
  git add editor/src-tauri/src/commands.rs
  git commit -m "feat(tauri): add sync_all_presets (paced) and get_live_state commands"
  ```

---

## Task 6: Rust — Auto-query on connect + register commands

**Files:**
- Modify: `editor/src-tauri/src/midi.rs`
- Modify: `editor/src-tauri/src/main.rs`

- [ ] **Step 1: Auto-send `GET_LIVE_STATE` after connect in `midi.rs`**

  Find the block in `connect()` that stores the output connection:
  ```rust
  {
      let mut s = state.lock().unwrap();
      s.output = Some(conn);
      s.input_active = true;
  }
  ```
  Add one line after the closing brace (lock is already released):
  ```rust
  {
      let mut s = state.lock().unwrap();
      s.output = Some(conn);
      s.input_active = true;
  }
  // Prime the editor with current hardware state immediately on connect.
  let _ = send_raw(&state, &crate::sysex::build_get_live_state());
  ```

- [ ] **Step 2: Update `invoke_handler` in `main.rs`**

  Replace the entire `invoke_handler` list:
  ```rust
  .invoke_handler(tauri::generate_handler![
      commands::list_midi_ports,
      commands::connect_midi,
      commands::send_cc,
      commands::get_preset,
      commands::put_preset,
      commands::get_all_presets,
      commands::set_active_preset,
      commands::set_mode,
      commands::set_fx_enabled,
  ])
  ```
  With:
  ```rust
  .invoke_handler(tauri::generate_handler![
      commands::list_midi_ports,
      commands::connect_midi,
      commands::send_cc,
      commands::get_preset,
      commands::put_preset,
      commands::sync_all_presets,
      commands::get_live_state,
      commands::set_active_preset,
      commands::set_mode,
      commands::set_fx_enabled,
  ])
  ```

- [ ] **Step 3: Build to verify**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx/editor/src-tauri && cargo build 2>&1 | grep -E "^error" | head -10
  ```
  Expected: no output.

- [ ] **Step 4: Commit**

  ```bash
  git add editor/src-tauri/src/midi.rs editor/src-tauri/src/main.rs
  git commit -m "feat(tauri): auto-query live state on connect; register new commands"
  ```

---

## Task 7: Editor — Add `valid` field to preset codec

**Files:**
- Modify: `editor/src/lib/presetCodec.ts`

- [ ] **Step 1: Add `valid` to `ParsedPreset` interface**

  Replace:
  ```ts
  export interface ParsedPreset {
    modMode: number;
    delayMode: number;
    reverbMode: number;
    modParams: number[];              // 7 normalised [0,1]
    delayParams: number[];            // 7 normalised [0,1]
    reverbParams: number[];           // 7 normalised [0,1]
    fxEnabled: [number, number, number]; // preserved for round-trip
  }
  ```
  With:
  ```ts
  export interface ParsedPreset {
    valid:     number;                   // raw[0]: 1 = populated, 0 = empty slot
    modMode:   number;
    delayMode: number;
    reverbMode: number;
    modParams:    number[];              // 7 normalised [0,1]
    delayParams:  number[];             // 7 normalised [0,1]
    reverbParams: number[];             // 7 normalised [0,1]
    fxEnabled: [number, number, number];
  }
  ```

- [ ] **Step 2: Return `valid` in `parsePresetSlot`**

  Replace the return statement in `parsePresetSlot`:
  ```ts
  return {
    modMode:    raw[1],
    delayMode:  raw[2],
    reverbMode: raw[3],
    modParams:    readFloats(view, 4,  7),
    delayParams:  readFloats(view, 32, 7),
    reverbParams: readFloats(view, 60, 7),
    fxEnabled: [raw[88], raw[89], raw[90]],
  };
  ```
  With:
  ```ts
  return {
    valid:      raw[0],
    modMode:    raw[1],
    delayMode:  raw[2],
    reverbMode: raw[3],
    modParams:    readFloats(view, 4,  7),
    delayParams:  readFloats(view, 32, 7),
    reverbParams: readFloats(view, 60, 7),
    fxEnabled: [raw[88], raw[89], raw[90]],
  };
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add editor/src/lib/presetCodec.ts
  git commit -m "feat(editor): expose valid byte in ParsedPreset for empty-slot detection"
  ```

---

## Task 8: Editor — `useMidi.ts` overhaul

**Files:**
- Modify: `editor/src/hooks/useMidi.ts`

- [ ] **Step 1: Add `LiveStateUpdate` interface and update `LoadedPresetResult`**

  At the top of the file, after the existing imports, update `LoadedPresetResult` to include `valid` (the `toResult` spreads `ParsedPreset` which now includes it):
  ```ts
  export interface LoadedPresetResult {
    valid:        number;
    modMode:      number;
    delayMode:    number;
    reverbMode:   number;
    modParams:    number[];
    delayParams:  number[];
    reverbParams: number[];
    fxEnabled:    [number, number, number];
    name:         string;
  }
  ```

  Add a new `LiveStateUpdate` interface after it:
  ```ts
  export interface LiveStateUpdate {
    bank:         number;
    slot:         number;
    valid:        number;
    modMode:      number;
    delayMode:    number;
    reverbMode:   number;
    modParams:    number[];
    delayParams:  number[];
    reverbParams: number[];
    fxEnabled:    [number, number, number];
  }
  ```

- [ ] **Step 2: Extend `SavePending` to carry preset data**

  Replace:
  ```ts
  interface SavePending {
    resolve: (ok: boolean) => void;
    timer: ReturnType<typeof setTimeout>;
  }
  ```
  With:
  ```ts
  interface SavePending {
    resolve: (ok: boolean) => void;
    timer:   ReturnType<typeof setTimeout>;
    bank:    number;
    slot:    number;
    name:    string;
    rawData: Uint8Array;
  }
  ```

- [ ] **Step 3: Add `liveState` and `syncProgress` state atoms inside the hook**

  Find the block of `useState` calls at the top of `useMidi()`. Add two new ones:
  ```ts
  const [liveState,    setLiveState]    = useState<LiveStateUpdate | null>(null);
  const [syncProgress, setSyncProgress] = useState<number | null>(null);
  ```

- [ ] **Step 4: Handle `cmd === 0x82` in the sysex listener**

  In the `useEffect` that calls `listen<number[]>("midi-sysex", ...)`, find the `if (cmd === 0x81)` / `else if (cmd === 0x83)` chain. Add a new branch:
  ```ts
  } else if (cmd === 0x82) {
    if (msg.length < 7) return;
    const bank    = msg[3];
    const slot    = msg[4];
    const encoded = msg.slice(5, msg.length - 1);
    const rawData = decode7bit(encoded);
    const parsed  = parsePresetSlot(rawData);
    setLiveState({ bank, slot, ...parsed });
  }
  ```

- [ ] **Step 5: Fix the `cmd === 0x83` ACK handler to update the preset cache**

  Replace the existing ACK handler block:
  ```ts
  } else if (cmd === 0x83) {
    if (msg.length < 6) return;
    const originalCmd = msg[3];
    const ok          = msg[4] === 0x00;
    if (originalCmd === 0x02 && savePending.current) {
      clearTimeout(savePending.current.timer);
      const { resolve } = savePending.current;
      savePending.current = null;
      resolve(ok);
    }
  }
  ```
  With:
  ```ts
  } else if (cmd === 0x83) {
    if (msg.length < 6) return;
    const originalCmd = msg[3];
    const ok          = msg[4] === 0x00;
    if (originalCmd === 0x02 && savePending.current) {
      clearTimeout(savePending.current.timer);
      const { resolve, bank, slot, name, rawData } = savePending.current;
      savePending.current = null;
      if (ok) {
        const updated: PresetData = { bank, slot, name, rawData };
        presetsRef.current[bank * 10 + slot] = updated;
        setPresets(prev => {
          const next = [...prev];
          next[bank * 10 + slot] = updated;
          return next;
        });
      }
      resolve(ok);
    }
  }
  ```

- [ ] **Step 6: Add `savePreset` pending data fields when creating the pending object**

  In `savePreset`, find:
  ```ts
  savePending.current = { resolve, timer };
  ```
  Replace with:
  ```ts
  savePending.current = { resolve, timer, bank, slot, name, rawData };
  ```

- [ ] **Step 7: Add sync progress listeners**

  Add a new `useEffect` after the existing `midi-ports-changed` effect:
  ```ts
  useEffect(() => {
    const u1 = listen<number>("midi-sync-progress", (e) => setSyncProgress(e.payload));
    const u2 = listen<null>("midi-sync-done",       ()  => setSyncProgress(null));
    return () => { u1.then((f) => f()); u2.then((f) => f()); };
  }, []);
  ```

- [ ] **Step 8: Replace `getAllPresets` with `syncAllPresets`**

  Remove:
  ```ts
  const getAllPresets = useCallback(() => {
    invoke("get_all_presets").catch((e) => {
      reportError(`Sync failed: ${e}`);
    });
  }, [reportError]);
  ```
  Add:
  ```ts
  const syncAllPresets = useCallback(() => {
    invoke("sync_all_presets").catch((e) => {
      reportError(`Sync failed: ${e}`);
    });
  }, [reportError]);
  ```

- [ ] **Step 9: Expose `setPresets` from the hook**

  The `setPresets` dispatcher is created by `useState` at the top of the hook. It's already available in the closure — just add it to the return object.

- [ ] **Step 10: Update the return object**

  Replace the existing return statement with:
  ```ts
  return {
    ports,
    connected,
    presets,
    midiError,
    liveState,
    syncProgress,
    refreshPorts,
    connect,
    reportError,
    sendCC,
    setMode,
    setFxEnabled,
    syncAllPresets,
    putPreset,
    savePreset,
    loadPreset,
    setPresets,
  };
  ```

- [ ] **Step 11: Commit**

  ```bash
  git add editor/src/hooks/useMidi.ts
  git commit -m "feat(editor): handle LIVE_STATE, fix save cache, add sync progress"
  ```

---

## Task 9: Editor — `App.tsx` wiring

**Files:**
- Modify: `editor/src/App.tsx`

- [ ] **Step 1: Update the `useMidi` import to include new exports**

  No import change needed. `midi.liveState` is inferred as `LiveStateUpdate | null` by TypeScript from the hook's return type. The existing import line is unchanged:
  ```ts
  import { useMidi, PresetData, LoadedPresetResult } from "./hooks/useMidi";
  ```

- [ ] **Step 2: Add `useEffect` to apply incoming `liveState` without echoing**

  Add after the existing `isDirty` declaration:
  ```ts
  useEffect(() => {
    if (!midi.liveState) return;
    const ls = midi.liveState;
    setModMode(ls.modMode);
    setDelayMode(ls.delayMode);
    setReverbMode(ls.reverbMode);
    setModParams([...ls.modParams]);
    setDelayParams([...ls.delayParams]);
    setReverbParams([...ls.reverbParams]);
    setFxEnabled(ls.fxEnabled.map(v => v !== 0) as [boolean, boolean, boolean]);
    setActivePreset({ bank: ls.bank, slot: ls.slot });
    const cached = midi.presets[ls.bank * 10 + ls.slot];
    const name   = cached?.name ?? "";
    setPresetName(name);
    setLoadedSnapshot({
      valid:        ls.valid,
      modMode:      ls.modMode,
      delayMode:    ls.delayMode,
      reverbMode:   ls.reverbMode,
      modParams:    [...ls.modParams],
      delayParams:  [...ls.delayParams],
      reverbParams: [...ls.reverbParams],
      fxEnabled:    ls.fxEnabled,
      name,
    });
    setSaveError(null);
  }, [midi.liveState]);
  ```

- [ ] **Step 3: Trigger sequential sync after connect**

  Replace `handleConnect`:
  ```ts
  const handleConnect = useCallback(async (portName: string) => {
    try {
      await midi.connect(portName);
      midi.syncAllPresets();
    } catch (e) { midi.reportError(`Connect failed: ${e}`); }
  }, [midi]);
  ```

- [ ] **Step 4: Fix `handleImportDone` to update local state**

  Replace:
  ```ts
  const handleImportDone = useCallback(
    (imported: (PresetData | null)[]) => {
      imported.forEach((p) => {
        if (!p) return;
        midi.putPreset(p.bank, p.slot, p.name, p.rawData);
      });
    },
    [midi]
  );
  ```
  With:
  ```ts
  const handleImportDone = useCallback(
    (imported: (PresetData | null)[]) => {
      const next = [...midi.presets];
      imported.forEach((p) => {
        if (!p) return;
        next[p.bank * 10 + p.slot] = p;
        midi.putPreset(p.bank, p.slot, p.name, p.rawData);
      });
      midi.setPresets(next);
    },
    [midi]
  );
  ```

- [ ] **Step 5: Fix `handlePresetSelect` to keep current state for empty slots**

  Replace the body of `handlePresetSelect`:
  ```ts
  const handlePresetSelect = useCallback(
    async (bank: number, slot: number) => {
      const result = await midi.loadPreset(bank, slot);
      if (!result) return;
      if (result.valid === 0) {
        // Empty slot: anchor the slot without resetting knobs to zero
        setActivePreset({ bank, slot });
        setLoadedSnapshot(null);
        setPresetName("");
      } else {
        setModMode(result.modMode);
        setDelayMode(result.delayMode);
        setReverbMode(result.reverbMode);
        setModParams([...result.modParams]);
        setDelayParams([...result.delayParams]);
        setReverbParams([...result.reverbParams]);
        setFxEnabled(result.fxEnabled.map(v => v !== 0) as [boolean, boolean, boolean]);
        setPresetName(result.name);
        setActivePreset({ bank, slot });
        setLoadedSnapshot({ ...result });
      }
      setSaveError(null);
    },
    [midi]
  );
  ```

- [ ] **Step 6: Fix `isDirty` to treat `null` snapshot as always dirty**

  Replace:
  ```ts
  const isDirty =
    activePreset !== null &&
    loadedSnapshot !== null &&
    (presetName !== loadedSnapshot.name ||
      modMode    !== loadedSnapshot.modMode    ||
      delayMode  !== loadedSnapshot.delayMode  ||
      reverbMode !== loadedSnapshot.reverbMode ||
      !paramsEqual(modParams,    loadedSnapshot.modParams)    ||
      !paramsEqual(delayParams,  loadedSnapshot.delayParams)  ||
      !paramsEqual(reverbParams, loadedSnapshot.reverbParams) ||
      fxEnabled.some((v, i) => v !== (loadedSnapshot.fxEnabled[i] !== 0)));
  ```
  With:
  ```ts
  const isDirty =
    activePreset !== null && (
      loadedSnapshot === null ||
      presetName !== loadedSnapshot.name ||
      modMode    !== loadedSnapshot.modMode    ||
      delayMode  !== loadedSnapshot.delayMode  ||
      reverbMode !== loadedSnapshot.reverbMode ||
      !paramsEqual(modParams,    loadedSnapshot.modParams)    ||
      !paramsEqual(delayParams,  loadedSnapshot.delayParams)  ||
      !paramsEqual(reverbParams, loadedSnapshot.reverbParams) ||
      fxEnabled.some((v, i) => v !== (loadedSnapshot.fxEnabled[i] !== 0))
    );
  ```

- [ ] **Step 7: Fix `handleSave` — remove `loadedSnapshot` guard and update snapshot with `valid: 1`**

  Replace the guard and the success path:
  ```ts
  // Old guard:
  if (!activePreset || !loadedSnapshot) return;

  // New guard (loadedSnapshot no longer required to save):
  if (!activePreset) return;
  ```

  In the success branch, add `valid: 1`:
  ```ts
  if (ok) {
    setLoadedSnapshot({
      valid:        1,
      modMode, delayMode, reverbMode,
      modParams:    [...modParams],
      delayParams:  [...delayParams],
      reverbParams: [...reverbParams],
      fxEnabled:    fxRaw,
      name:         presetName,
    });
  }
  ```

  Also remove `loadedSnapshot` from the `useCallback` dependency array since it's no longer read inside `handleSave`.

- [ ] **Step 8: Wire `syncProgress` and `syncAllPresets` to `PresetHeader`**

  Replace the entire `<PresetHeader ... />` element in the return JSX:
  ```tsx
  <PresetHeader
    connected={midi.connected}
    ports={midi.ports}
    onConnect={handleConnect}
    onRefresh={midi.refreshPorts}
    activePreset={activePreset}
    presetName={presetName}
    isDirty={isDirty}
    isSaving={isSaving}
    saveError={saveError}
    midiError={midi.midiError}
    onNameChange={setPresetName}
    onSave={handleSave}
    onSyncAll={midi.syncAllPresets}
    onExport={() => setExportOpen(true)}
    onImport={() => setExportOpen(true)}
    syncProgress={midi.syncProgress}
  />
  ```

- [ ] **Step 9: Commit**

  ```bash
  git add editor/src/App.tsx
  git commit -m "feat(editor): wire live state, fix import/save/load flows, sync on connect"
  ```

---

## Task 10: Editor — `PresetHeader.tsx` sync progress bar

**Files:**
- Modify: `editor/src/components/PresetHeader.tsx`

- [ ] **Step 1: Add `syncProgress` to `PresetHeaderProps`**

  Add to the interface:
  ```ts
  interface PresetHeaderProps {
    ...existing fields...
    syncProgress: number | null;
  }
  ```
  And destructure it in the component signature:
  ```ts
  export function PresetHeader({
    connected, ports, onConnect, onRefresh,
    activePreset, presetName, isDirty, isSaving, saveError, midiError,
    onNameChange, onSave, onSyncAll, onExport, onImport,
    syncProgress,
  }: PresetHeaderProps) {
  ```

- [ ] **Step 2: Wrap the existing `<div>` in an outer column div and append the progress bar**

  The component's return starts with:
  ```tsx
  return (
    <div className="flex items-center gap-2 bg-zinc-900 border border-zinc-800 rounded-lg px-3 py-2">
  ```
  Change that opening to add an outer wrapper:
  ```tsx
  return (
    <div className="flex flex-col">
      <div className="flex items-center gap-2 bg-zinc-900 border border-zinc-800 rounded-lg px-3 py-2">
  ```

  Then the component's return currently ends with:
  ```tsx
    </div>
  );
  ```
  Change that closing to add the progress bar and close the outer wrapper:
  ```tsx
      </div>
      {syncProgress !== null && (
        <div className="h-0.5 bg-zinc-800 rounded-full mt-0.5 mx-1">
          <div
            className="h-full bg-cyan-500 rounded-full transition-all duration-75"
            style={{ width: `${syncProgress}%` }}
          />
        </div>
      )}
    </div>
  );
  ```

- [ ] **Step 3: Commit**

  ```bash
  git add editor/src/components/PresetHeader.tsx
  git commit -m "feat(editor): add sync progress bar to PresetHeader"
  ```

---

## Task 11: Integration Verification

- [ ] **Step 1: Build and flash firmware**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx && make -j4 && make program-dfu
  ```

- [ ] **Step 2: Start the editor**

  ```bash
  cd /Users/bbalazs/daisy/multi-fx/editor && npm run tauri dev
  ```

- [ ] **Step 3: Verify connect → live state**

  Connect to the hardware MIDI port. Expected:
  - Active preset slot is highlighted immediately in the browser (within ~200 ms)
  - Knob positions in editor reflect hardware's current state
  - Thin cyan progress bar appears and fills over ~2.5 s
  - All 100 preset slots populate with names (or show "—" if empty)

- [ ] **Step 4: Verify hardware knob → editor mirror**

  Turn any parameter encoder on the hardware. Expected: the corresponding knob in the editor updates within ~200 ms without any save action.

- [ ] **Step 5: Verify editor knob → hardware**

  Move a knob in the editor (drag a parameter). Expected: hardware audio reflects the change in real time (existing CC path, unchanged).

- [ ] **Step 6: Verify save to populated slot**

  Load a preset by clicking it in the browser. Change a parameter. Click Save. Expected:
  - Save button briefly shows "Saving…"
  - After ACK, browser shows updated name, dirty indicator disappears

- [ ] **Step 7: Verify save to empty slot**

  Click an empty slot in the browser. Expected: knobs stay at current positions (not reset to zero), slot becomes active. Type a name, click Save. Expected: slot now shows the name in the browser.

- [ ] **Step 8: Verify import updates browser immediately**

  Export current presets, clear one slot name, re-import the file. Expected: browser updates immediately without needing Sync All.

- [ ] **Step 9: Verify hardware browse-mode save**

  On hardware: hold TAP (1 s) → LEDs blink → press DELAY footswitch to save. Expected: display shows "SAVED" for ~1 s, editor's browser updates the slot name within ~200 ms (via LIVE_STATE broadcast).
