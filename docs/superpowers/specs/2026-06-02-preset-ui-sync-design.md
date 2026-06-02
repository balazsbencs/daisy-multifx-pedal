# Preset UI Sync & Display Fix — Design Spec

**Date:** 2026-06-02  
**Status:** Approved

---

## Problem

Three related failures make preset management unusable:

1. **Display shows wrong info.** The firmware status row shows a CPU % that is no longer needed. The corner slot label (`P1`…`P8`) uses `% 8` instead of `% 10` and omits the bank entirely.
2. **Editor doesn't know the active preset.** On connect, all 100 preset slots show as empty (`null`). The `activePreset` in App stays `null` indefinitely, disabling the Save button and showing "No preset loaded".
3. **Save appears broken.** Save is structurally correct (SysEx PUT + ACK round-trip) but the button is always disabled because of problem 2.

---

## Solution Overview

Three coordinated changes — firmware display, firmware protocol, editor frontend.

---

## 1. Firmware: Display Changes

**File:** `src/display/display_manager.cpp` and `display_manager.h`

### Remove
- `cpu_usage_` member variable
- `SetCpuUsage()` method and all call sites
- The CPU render block in `Render()` (currently at `CPU_X=90, STATUS_Y=282`)
- The broken corner slot label (`SLOT_X=200, SLOT_Y=33`)

### Add
In the status row (`STATUS_Y=282`), at `CPU_X=90` (repurpose the freed space), always render:

```
B{bank} P{slot:02d}
```

Examples: `B0 P00`, `B3 P07`, `B9 P09`

Bank and slot are derived from the `preset_slot` parameter (which is the global index 0–99):
```c
const int disp_bank = preset_slot / PRESET_SLOTS_PER_BANK;
const int disp_slot = preset_slot % PRESET_SLOTS_PER_BANK;
```

**Layout:** 7 chars × 6px (`Font_6x8`) = 42px. Starts at `CPU_X=90`, ends at x=132 — well clear of `PRESET_EVT_X=174`. Always visible.

**Update layout constants:** Remove `CPU_X` and `CPU_WARN_PCT` from `display_layout.h`. Add `PRESET_ID_X = 90` (same value, clearer name).

**Update `Update()` signature:** Remove the `float cpu_usage` parameter (if it was ever passed separately) — `SetCpuUsage` is the current path, so removing the member is sufficient.

---

## 2. Firmware: GET_STATUS Command (0x06)

**File:** `src/midi/midi_handler.cpp`

Add one new case to the `HandleSysEx` switch:

```c
case 0x06u: { // GET_STATUS — returns current active bank/slot via ACK
    SendAck(cmd, true);
    break;
}
```

`SendAck` already encodes `GetActiveBank()` and `GetActiveSlot()` in bytes 5–6 of the response frame:
```
F0 7D 83 06 00 <activeBank> <activeSlot> F7
```

No side-effects. No parameters required. Frame: `F0 7D 06 F7` (4 bytes).

**File:** `editor/src-tauri/src/sysex.rs`

Add builder:
```rust
pub fn build_get_status() -> Vec<u8> {
    vec![0xF0, 0x7D, 0x06, 0xF7]
}
```

**File:** `editor/src-tauri/src/commands.rs`

Add command:
```rust
#[tauri::command]
pub fn get_status(state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_get_status())
}
```

Register in `main.rs` invoke handler.

---

## 3. Editor: Auto-Sync on Connect

**File:** `editor/src/hooks/useMidi.ts`

### 3a. Track device active preset

Add state:
```typescript
const [deviceActivePreset, setDeviceActivePreset] =
  useState<{ bank: number; slot: number } | null>(null);
```

In the existing SysEx ACK handler (`cmd === 0x83`), always extract bytes 5–6:
```typescript
if (msg.length >= 8) {
  setDeviceActivePreset({ bank: msg[5], slot: msg[6] });
}
```

This runs on every ACK — GET_STATUS, PUT_PRESET, SET_ACTIVE, SET_MODE, SET_FX_ENABLED — keeping the editor permanently in sync with the device's active preset.

Return `deviceActivePreset` from the hook.

### 3b. Add `getStatus()` function

```typescript
const getStatus = useCallback(() => {
  invoke("get_status").catch((e) => { reportError(`Status query failed: ${e}`); });
}, [reportError]);
```

### 3c. Auto-sync on connect

In the `connect()` function, after the output connection succeeds and `setConnected(true)`:

1. Call `getStatus()` — fast, gets activeBank/activeSlot immediately.
2. Fire-and-forget a background sequential preset sync — do NOT await it inside `connect()`. Fetches all 100 presets one at a time with ~20ms spacing to avoid overwhelming the firmware's USB TX FIFO (12,400 bytes total if sent at once would overflow the ~512-byte FIFO):

```typescript
const syncAllPresets = useCallback(async () => {
  for (let bank = 0; bank < 10; bank++) {
    for (let slot = 0; slot < 10; slot++) {
      invoke("get_preset", { bank, slot }).catch(() => {});
      await new Promise<void>((r) => setTimeout(r, 20));
    }
  }
}, []);
```

The existing `midi-sysex` handler for `cmd === 0x81` (PRESET_DATA) already populates `presets` state as responses arrive — no changes needed there. The browser updates incrementally as data flows in.

Also call `syncAllPresets()` from the port-reconnect handler in the `midi-ports-changed` listener (same place `setConnected(true)` is called on reappear).

### 3d. Register `get_status` command in Rust

`main.rs` invoke handler must include `commands::get_status`.

---

## 4. Editor: Auto-Load Active Preset

**File:** `editor/src/App.tsx`

Add a `useEffect` that reacts to `midi.deviceActivePreset`:

```typescript
useEffect(() => {
  if (midi.deviceActivePreset && activePreset === null) {
    handlePresetSelect(
      midi.deviceActivePreset.bank,
      midi.deviceActivePreset.slot,
    );
  }
}, [midi.deviceActivePreset]);
```

Condition `activePreset === null` means this only triggers on first connect (not on every subsequent ACK from normal editing operations). Once a preset is loaded, the user is in control.

---

## Data Flow After This Change

```
Connect
  → invoke get_status → ACK(0x06) → setDeviceActivePreset({bank, slot})
  → useEffect detects deviceActivePreset, activePreset=null → handlePresetSelect
    → invoke set_active_preset + get_preset → PRESET_DATA → load params + setActivePreset
  → syncAllPresets (background, 2s) → 100× PRESET_DATA → presets[] populated
  → PresetBrowser shows named slots, active one highlighted

User edits knobs
  → isDirty = true → Save button enabled

User clicks Save
  → invoke put_preset → ACK(0x02, ok=true, activeBank, activeSlot)
    → savePending resolved → setLoadedSnapshot → isDirty = false
    → setDeviceActivePreset updated (no-op since same slot)
```

---

## Files Changed

| File | Change |
|------|--------|
| `src/display/display_manager.h` | Remove `SetCpuUsage`, `cpu_usage_` |
| `src/display/display_manager.cpp` | Remove CPU render, add B/P label, fix slot computation |
| `src/display/display_layout.h` | Remove `CPU_X`, `CPU_WARN_PCT`; add `PRESET_ID_X` |
| `src/midi/midi_handler.cpp` | Add case 0x06 GET_STATUS |
| `editor/src-tauri/src/sysex.rs` | Add `build_get_status()` |
| `editor/src-tauri/src/commands.rs` | Add `get_status` command |
| `editor/src-tauri/src/main.rs` | Register `get_status` in invoke handler |
| `editor/src/hooks/useMidi.ts` | Add `deviceActivePreset`, `getStatus`, `syncAllPresets`, update `connect` and ACK handler |
| `editor/src/App.tsx` | Add `useEffect` for `deviceActivePreset` auto-load |

---

## Out of Scope

- Changing the SysEx protocol version or framing
- Adding undo/redo for preset edits
- Changing how HOLD or SAVED/LOAD events render in the status row
