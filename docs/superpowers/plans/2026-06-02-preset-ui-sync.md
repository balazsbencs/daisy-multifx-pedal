# Preset UI Sync & Display Fix — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix preset management so the device display shows bank + slot, and the desktop editor auto-syncs and tracks the active preset on connect.

**Architecture:** Three coordinated layers — firmware display render + new GET_STATUS SysEx command (0x06), Rust Tauri backend exposing the new command, TypeScript frontend auto-loading on connect and tracking device active preset via ACK bytes 5–6.

**Tech Stack:** C++ (Daisy firmware, no test harness), Rust (Tauri backend, midir), TypeScript/React (Tauri frontend, useMidi hook)

**Note:** This project has no automated test suite. Verification steps are build checks + manual hardware testing.

**Spec:** `docs/superpowers/specs/2026-06-02-preset-ui-sync-design.md`

---

## File Map

| File | Change |
|------|--------|
| `src/display/display_layout.h` | Remove `SLOT_X`, `SLOT_Y`, `CPU_X`, `CPU_WARN_PCT`; add `PRESET_ID_X` |
| `src/display/display_manager.h` | Remove `cpu_usage_` member and `SetCpuUsage()` |
| `src/display/display_manager.cpp` | Remove CPU render + corner slot label; add B/P label in status row |
| `src/main.cpp` | Remove `cpu_accum_`, `cpu_accum_count`, CPU averaging block |
| `src/midi/midi_handler.cpp` | Add `case 0x06u` GET_STATUS |
| `editor/src-tauri/src/sysex.rs` | Add `build_get_status()` |
| `editor/src-tauri/src/commands.rs` | Add `get_status` command |
| `editor/src-tauri/src/main.rs` | Register `get_status` in invoke handler |
| `editor/src/hooks/useMidi.ts` | `deviceActivePreset` state, ACK tracking, `getStatus`, `syncAllPresets`, update `connect` |
| `editor/src/App.tsx` | `useEffect` auto-loads active preset on connect |

---

## Task 1 — Display Layout Constants

**Files:**
- Modify: `src/display/display_layout.h`

- [ ] **Remove SLOT_X, SLOT_Y, CPU_X, CPU_WARN_PCT. Add PRESET_ID_X.**

Replace the header row and status row constant blocks:

```cpp
// ── Header (y=0..55, h=56) ────────────────────────────────────────────────────
constexpr uint16_t HEADER_H       = 56;

// Tab strip (y=0..27, h=28): three 80px tabs
constexpr uint16_t TAB_H          = 28;
constexpr uint16_t TAB_W          = 80;
constexpr uint16_t TAB_TEXT_Y     = 5;      // (28-18)/2: centre Font_11x18 (18px) in 28px
constexpr uint16_t TAB_TEXT_X_OFF = 23;     // (80-33)/2: centre "MOD"/"DLY"/"REV" (3×11px)

// Mode name (y=28..55, h=28)
constexpr uint16_t MODE_Y  = 33;            // (28-18)/2 + 28
constexpr uint16_t MODE_X  = 4;
```

And update the status row block:

```cpp
// ── Status row (y=280..291, h=12) ─────────────────────────────────────────────
constexpr uint16_t STATUS_Y       = 282;
constexpr uint16_t HOLD_X         = 4;
constexpr uint16_t PRESET_ID_X    = 90;     // "B3 P07" — bank + slot
constexpr uint16_t PRESET_EVT_X   = 174;
```

- [ ] **Build firmware to confirm no undeclared-identifier errors.**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | head -40
```

Expected: compile errors from `display_manager.cpp` referencing the removed constants (`SLOT_X`, `CPU_X`, `CPU_WARN_PCT`). That is expected — they'll be fixed in Task 2 and 3.

- [ ] **Commit.**

```bash
git add src/display/display_layout.h
git commit -m "refactor: replace CPU_X/SLOT_X layout constants with PRESET_ID_X"
```

---

## Task 2 — Display Manager Interface

**Files:**
- Modify: `src/display/display_manager.h`

- [ ] **Remove cpu_usage_ member and SetCpuUsage() from display_manager.h.**

In the `public:` section, delete this line:
```cpp
    void SetCpuUsage(float usage) { cpu_usage_ = usage; }
```

In the `private:` section, delete this line:
```cpp
    float cpu_usage_ = 0.0f;
```

Also remove the `Update()` forward declaration's dependency: the Update signature itself does not change (CPU was fed via `SetCpuUsage`, not a parameter), so no signature change needed.

- [ ] **Commit.**

```bash
git add src/display/display_manager.h
git commit -m "refactor: remove SetCpuUsage from DisplayManager"
```

---

## Task 3 — Display Manager Render

**Files:**
- Modify: `src/display/display_manager.cpp`

- [ ] **Remove the corner slot label block.**

Delete these 4 lines from the `Render()` function (they appear after the mode name DrawText call):

```cpp
    // Preset slot "P1".."P8"
    char slot_buf[3] = { 'P', static_cast<char>('1' + (preset_slot % 8)), 0 };
    DisplayRenderer::DrawText(layout::SLOT_X, layout::SLOT_Y,
                              slot_buf, kColorWhite, kColorBlack, Font_7x10);
```

- [ ] **Replace the CPU render block with the B/P label.**

In the `// ── Status row` section, find and delete this entire block:

```cpp
    // CPU usage: always shown, centre of status row
    {
        char cpu_buf[8];
        const int pct = static_cast<int>(cpu_usage_ * 100.0f + 0.5f);
        cpu_buf[0] = 'C'; cpu_buf[1] = 'P'; cpu_buf[2] = 'U'; cpu_buf[3] = ' ';
        if (pct >= 100) {
            cpu_buf[4] = '1'; cpu_buf[5] = '0'; cpu_buf[6] = '0'; cpu_buf[7] = '\0';
        } else {
            cpu_buf[4] = static_cast<char>('0' + pct / 10);
            cpu_buf[5] = static_cast<char>('0' + pct % 10);
            cpu_buf[6] = '%'; cpu_buf[7] = '\0';
        }
        const uint16_t cpu_color = (pct >= layout::CPU_WARN_PCT) ? kColorRed : kColorWhite;
        DisplayRenderer::DrawText(layout::CPU_X, layout::STATUS_Y, cpu_buf,
                                  cpu_color, kColorBlack, Font_6x8);
    }
```

Replace it with:

```cpp
    // Preset bank and slot — always shown, e.g. "B3 P07"
    {
        const int disp_bank = preset_slot / PRESET_SLOTS_PER_BANK;
        const int disp_slot = preset_slot % PRESET_SLOTS_PER_BANK;
        char id_buf[7];
        id_buf[0] = 'B';
        id_buf[1] = static_cast<char>('0' + disp_bank);
        id_buf[2] = ' ';
        id_buf[3] = 'P';
        id_buf[4] = static_cast<char>('0' + disp_slot / 10);
        id_buf[5] = static_cast<char>('0' + disp_slot % 10);
        id_buf[6] = '\0';
        DisplayRenderer::DrawText(layout::PRESET_ID_X, layout::STATUS_Y,
                                  id_buf, kColorWhite, kColorBlack, Font_6x8);
    }
```

- [ ] **Build firmware — expect only main.cpp errors now.**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | grep -E "error:|warning:" | head -20
```

Expected: errors only from `main.cpp` referencing the removed `SetCpuUsage()`. Display files should be clean.

- [ ] **Commit.**

```bash
git add src/display/display_manager.cpp
git commit -m "feat: replace CPU label with B/P bank+slot indicator on display"
```

---

## Task 4 — Remove CPU Averaging from main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Remove the two static CPU accumulator variables.**

Find and delete these two lines (they are static locals declared just before the `while (true)` loop):

```cpp
    static float    cpu_accum_       = 0.0f;
    static int      cpu_accum_count  = 0;
```

- [ ] **Remove the CPU averaging + SetCpuUsage block inside the display update section.**

Find and delete these 7 lines inside the `if (now - display_last_ms >= DISPLAY_UPDATE_MS)` block:

```cpp
            cpu_accum_ += AudioEngine::GetCpuUsage();
            cpu_accum_count++;
            if (cpu_accum_count >= CPU_AVERAGE_FRAMES) {
                display.SetCpuUsage(cpu_accum_ / static_cast<float>(cpu_accum_count));
                cpu_accum_      = 0.0f;
                cpu_accum_count = 0;
            }
```

After deletion the display block reads:

```cpp
        if (now - display_last_ms >= DISPLAY_UPDATE_MS) {
            display_last_ms = now;
            display.Update(active_page, shift,
                           cur_mod, cur_delay, cur_reverb,
                           buf.mod, buf.delay, buf.reverb,
                           fx_enabled, hold_active,
                           preset_bank * PRESET_SLOTS_PER_BANK + preset_slot,
                           PresetUiEvent::None, now);
        }
```

- [ ] **Build firmware — should be clean.**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | grep -E "^.*error:" | head -20
```

Expected: no errors. Warnings about `CPU_AVERAGE_FRAMES` being unused if it's only used in the removed block — if so, remove it from `src/config/constants.h` too:

```cpp
// DELETE this line from constants.h if the build warns about it:
constexpr int      CPU_AVERAGE_FRAMES = 1000u / DISPLAY_UPDATE_MS;
```

- [ ] **Commit.**

```bash
git add src/main.cpp src/config/constants.h
git commit -m "refactor: remove CPU usage averaging from main loop"
```

---

## Task 5 — Firmware GET_STATUS Command

**Files:**
- Modify: `src/midi/midi_handler.cpp`

- [ ] **Add case 0x06 GET_STATUS to HandleSysEx.**

In the `switch (cmd)` block in `HandleSysEx`, add after the existing `case 0x05u` block and before `default:`:

```cpp
        case 0x06u: { // GET_STATUS — responds with current active bank/slot in ACK
            SendAck(cmd, true);
            break;
        }
```

The `SendAck` already encodes `GetActiveBank()` and `GetActiveSlot()` in bytes 5–6:
```
F0 7D 83 06 00 <activeBank> <activeSlot> F7
```

- [ ] **Build firmware — must be clean.**

```bash
cd /Users/bbalazs/daisy/multi-fx && make -j4 2>&1 | grep -E "^.*error:" | head -10
```

Expected: zero errors.

- [ ] **Commit.**

```bash
git add src/midi/midi_handler.cpp
git commit -m "feat: add GET_STATUS (0x06) SysEx command returning active bank/slot"
```

---

## Task 6 — Flash Firmware

- [ ] **Put Daisy into Daisy Bootloader DFU mode: press RESET only (do NOT hold BOOT). Then within 2 seconds run:**

```bash
cd /Users/bbalazs/daisy/multi-fx && make program-dfu
```

Expected output ends with `File downloaded successfully`.

- [ ] **Verify display on hardware.**

Power-cycle the pedal. The status row should show `B0 P00` (or whatever the stored active preset is). No CPU% label should appear. The corner slot indicator (`P1`…) should be gone.

---

## Task 7 — Rust: build_get_status

**Files:**
- Modify: `editor/src-tauri/src/sysex.rs`

- [ ] **Add build_get_status() function.**

Add after `build_cc`:

```rust
pub fn build_get_status() -> Vec<u8> {
    vec![0xF0, 0x7D, 0x06, 0xF7]
}
```

- [ ] **Verify Rust compiles.**

```bash
cd /Users/bbalazs/daisy/multi-fx/editor/src-tauri && cargo check 2>&1 | grep -E "^error" | head -10
```

Expected: no errors.

- [ ] **Commit.**

```bash
git add editor/src-tauri/src/sysex.rs
git commit -m "feat: add build_get_status sysex builder"
```

---

## Task 8 — Rust: get_status Command + Registration

**Files:**
- Modify: `editor/src-tauri/src/commands.rs`
- Modify: `editor/src-tauri/src/main.rs`

- [ ] **Add get_status command to commands.rs.**

Add after `set_fx_enabled`:

```rust
#[tauri::command]
pub fn get_status(state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_get_status())
}
```

- [ ] **Register get_status in main.rs invoke handler.**

In `main.rs`, add `commands::get_status` to the `tauri::generate_handler!` macro:

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
            commands::get_status,
        ])
```

- [ ] **Verify Rust compiles clean.**

```bash
cd /Users/bbalazs/daisy/multi-fx/editor/src-tauri && cargo check 2>&1 | grep -E "^error" | head -10
```

Expected: no errors.

- [ ] **Commit.**

```bash
git add editor/src-tauri/src/commands.rs editor/src-tauri/src/main.rs
git commit -m "feat: expose get_status Tauri command"
```

---

## Task 9 — useMidi: deviceActivePreset + ACK Tracking + Connect Flow

**Files:**
- Modify: `editor/src/hooks/useMidi.ts`

- [ ] **Add deviceActivePreset state.**

After the `midiError` state declaration, add:

```typescript
const [deviceActivePreset, setDeviceActivePreset] =
  useState<{ bank: number; slot: number } | null>(null);
```

- [ ] **Update the ACK handler to always extract active bank/slot.**

In the `midi-sysex` useEffect, find the `} else if (cmd === 0x83) {` block. Replace it with:

```typescript
      } else if (cmd === 0x83) {
        if (msg.length < 8) return;
        const originalCmd = msg[3];
        const ok          = msg[4] === 0x00;
        setDeviceActivePreset({ bank: msg[5], slot: msg[6] });
        if (originalCmd === 0x02 && savePending.current) {
          clearTimeout(savePending.current.timer);
          const { resolve } = savePending.current;
          savePending.current = null;
          resolve(ok);
        }
      }
```

Note: changed `msg.length < 6` to `msg.length < 8` (ACK is always 8 bytes: F0 7D 83 cmd ok bank slot F7). The `setDeviceActivePreset` call runs for every ACK, keeping the editor permanently in sync.

- [ ] **Add getStatus and syncAllPresets BEFORE the connect callback.**

`connect` calls these two functions, so they must be declared first. Insert both between the `refreshPorts` callback and the `connect` callback (around line 64, before `const connect = useCallback`):

```typescript
  const getStatus = useCallback(() => {
    invoke("get_status").catch((e) => { reportError(`Status query failed: ${e}`); });
  }, [reportError]);

  const syncAllPresets = useCallback(async () => {
    for (let bank = 0; bank < 10; bank++) {
      for (let slot = 0; slot < 10; slot++) {
        invoke("get_preset", { bank, slot }).catch(() => {});
        await new Promise<void>((r) => setTimeout(r, 20));
      }
    }
  }, []);
```

- [ ] **Call getStatus and syncAllPresets after connect.**

In the `connect` callback (which now appears after the two new callbacks above), replace:

```typescript
  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    connectedPort.current = portName;
    setConnected(true);
  }, []);
```

With:

```typescript
  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    connectedPort.current = portName;
    setConnected(true);
    getStatus();
    syncAllPresets();
  }, [getStatus, syncAllPresets]);
```

- [ ] **Also call getStatus and syncAllPresets on auto-reconnect.**

In the `midi-ports-changed` `useEffect`, update the reconnect block AND the dep array. Replace the entire effect:

```typescript
  useEffect(() => {
    const unlisten = listen<string[]>("midi-ports-changed", (event) => {
      const newPorts = event.payload;
      setPorts(newPorts);
      const port = connectedPort.current;
      if (!port) return;
      if (newPorts.includes(port)) {
        invoke("connect_midi", { portName: port })
          .then(() => {
            setConnected(true);
            getStatus();
            syncAllPresets();
          })
          .catch(() => {
            connectedPort.current = null;
            setConnected(false);
          });
      } else {
        setConnected(false);
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, [getStatus, syncAllPresets]);
```

- [ ] **Return deviceActivePreset from the hook.**

In the `return` object at the bottom of `useMidi`, add `deviceActivePreset`:

```typescript
  return {
    ports,
    connected,
    presets,
    midiError,
    deviceActivePreset,
    refreshPorts,
    connect,
    reportError,
    sendCC,
    setMode,
    setFxEnabled,
    getAllPresets,
    putPreset,
    savePreset,
    loadPreset,
  };
```

- [ ] **Verify TypeScript compiles.**

```bash
cd /Users/bbalazs/daisy/multi-fx/editor && npm run build 2>&1 | grep -E "error TS" | head -20
```

Expected: no TypeScript errors.

- [ ] **Commit.**

```bash
git add editor/src/hooks/useMidi.ts
git commit -m "feat: track deviceActivePreset from ACK, auto-sync on connect"
```

---

## Task 10 — App.tsx: Auto-Load Active Preset on Connect

**Files:**
- Modify: `editor/src/App.tsx`

- [ ] **Add useEffect to auto-load when deviceActivePreset arrives.**

After the existing `useEffect(() => { midi.refreshPorts(); }, []);`, add:

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

The `activePreset === null` guard ensures this only fires on initial connect, not on every subsequent ACK during normal editing.

- [ ] **Build the editor.**

```bash
cd /Users/bbalazs/daisy/multi-fx/editor && npm run build 2>&1 | grep -E "error TS|Error" | head -20
```

Expected: no errors.

- [ ] **Commit.**

```bash
git add editor/src/App.tsx
git commit -m "feat: auto-load active preset from device on connect"
```

---

## Task 11 — End-to-End Verification

- [ ] **Start the Tauri dev app.**

```bash
cd /Users/bbalazs/daisy/multi-fx/editor && npm run tauri dev
```

- [ ] **Connect the pedal and verify the connect flow.**

1. Open the app — see port list.
2. Select the Daisy MIDI port and click Connect.
3. Expected within ~2s:
   - `PresetHeader` shows the preset name and `B{n}·{mm}` badge of whichever preset was active on the device.
   - `PresetBrowser` populates incrementally — preset names appear slot by slot over ~2s.
   - Save button remains disabled (no edits yet).

- [ ] **Verify dirty tracking and save.**

1. Move a knob in any StageCard. The `●` dirty indicator should appear in `PresetHeader`.
2. Click Save. The `●` disappears. The firmware's display should briefly flash `SAVED` in the status row.
3. Power-cycle the pedal. Reconnect. The preset should reload with the saved values.

- [ ] **Verify display on hardware.**

Cycle through presets using the footswitch. The status row on the pedal display should update to `B0 P00`, `B0 P01`, etc. (or whichever bank/slot pattern your presets use). No CPU% should be visible.
