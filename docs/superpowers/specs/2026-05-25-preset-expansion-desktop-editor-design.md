# Preset Expansion + Desktop Editor Design

**Date:** 2026-05-25
**Status:** Approved

## Overview

Two deliverables connected over USB MIDI:

1. **Firmware:** Expand from 8 to 100 presets (10 banks × 10 slots), footswitch-based preset mode, custom QSPI storage, SysEx protocol handler.
2. **Desktop app:** Tauri 2.x editor with real-time parameter control (CC) and preset library management (SysEx).

Everything not listed under "What changes" is untouched: audio engine, DSP modes, CC 14–34 param handling, tap tempo, MIDI clock.

---

## Section 1: Architecture

```
┌─────────────────────────────────┐        USB MIDI       ┌──────────────────────────────┐
│         Daisy Firmware          │ ◄───────────────────► │      Tauri Desktop App        │
│                                 │                        │                              │
│  • 100 presets, 10 banks of 10  │  CC 14–34 (real-time) │  • shadcn/ui + knob panel    │
│  • Custom QSPI preset store     │  SysEx (preset ops)   │  • Preset browser/librarian  │
│  • SysEx handler in MIDI layer  │                        │  • midir Rust MIDI backend   │
│  • Footswitch preset mode UI    │                        │  • React + TypeScript        │
└─────────────────────────────────┘                        └──────────────────────────────┘
```

---

## Section 2: QSPI Preset Storage

Replaces `MultiPresetManager` + libDaisy `PersistentStorage`. Custom QSPI layout at `0x007E0000`:

```
0x007E0000  Header sector (4KB)
            magic(4), version(2), active_bank(1), active_slot(1), crc(4)
            name[100][12] = 1,200 bytes of null-terminated preset names

0x007E1000  Preset data — 3 × 4KB sectors
            100 × 92 bytes = 9,200 bytes (packed, no per-slot padding)

0x007E4000  Live state — 1 × 4KB sector
            One MultiPresetSlot (92 bytes) — current unsaved working state

Total: 5 sectors = 20KB
```

**New class:** `QspiPresetStore` (`src/presets/qspi_preset_store.{h,cpp}`)

```cpp
bool LoadSlot(int bank, int slot, MultiPresetSlot& out);
bool SaveSlot(int bank, int slot, const MultiPresetSlot& data);
bool LoadHeader(PresetHeader& out);
bool SaveHeader(const PresetHeader& in);
bool LoadLiveState(MultiPresetSlot& out);
bool SaveLiveState(const MultiPresetSlot& data);
```

`SaveSlot` locates the 4KB sector containing the target preset (up to 44 presets share a sector in the packed layout), reads it into a 4KB stack/static buffer, modifies the one slot, erases the sector, and rewrites it. Save latency ~50ms — acceptable since saves are always user-triggered. `MultiPresetSlot` struct (92 bytes) is unchanged; `kVersion` bumped.

`constants.h` changes:
- `PRESET_SLOT_COUNT = 100`
- `PRESET_BANK_COUNT = 10` (new)
- `PRESET_SLOTS_PER_BANK = 10` (new)

---

## Section 3: SysEx Protocol

Manufacturer ID: `0x7D` (non-commercial, no registration required).

Frame envelope: `F0 7D <cmd> [payload...] F7`

Binary payload uses standard MIDI 7-bit encoding: every 7 bytes of binary become 8 SysEx bytes (one overhead byte carries the 7 MSBs).

### Host → Device Commands

| Cmd | Name | Payload |
|-----|------|---------|
| `01` | `GET_PRESET` | `bank(1) slot(1)` |
| `02` | `PUT_PRESET` | `bank(1) slot(1) name[12] data[92]` (7-bit encoded) |
| `03` | `GET_HEADER` | *(none)* |
| `04` | `SET_ACTIVE` | `bank(1) slot(1)` |
| `05` | `GET_ALL` | *(none)* — device streams 100 × `PRESET_DATA` |
| `06` | `PUT_ALL` | host streams 100 × `PUT_PRESET` frames; device sends `ACK` after each one |
| `07` | `SET_MODE` | `stage(1) mode_index(1)` — stage: 0=mod, 1=delay, 2=reverb |

### Device → Host Responses

| Cmd | Name | Payload |
|-----|------|---------|
| `81` | `PRESET_DATA` | `bank slot name[12] data[92]` (7-bit encoded) |
| `82` | `HEADER_DATA` | `active_bank active_slot name[100][12]` |
| `83` | `ACK` | `cmd_echo status` (0=ok, 1=err) |

Real-time parameter control uses existing CC 14–34 — no SysEx involved.

---

## Section 4: Firmware Changes

### Files changed / added

| File | Change |
|------|--------|
| `src/config/constants.h` | Add `PRESET_BANK_COUNT`, `PRESET_SLOTS_PER_BANK`; `PRESET_SLOT_COUNT = 100` |
| `src/presets/qspi_preset_store.h` | New — `QspiPresetStore` class |
| `src/presets/qspi_preset_store.cpp` | New — QSPI read/write/erase implementation |
| `src/presets/preset_manager.h/.cpp` | Removed |
| `src/midi/midi_handler.h/.cpp` | Add `HandleSysEx()`, add `SET_MODE` dispatch |
| `src/main.cpp` | Preset mode state machine, footswitch UX, bank/slot nav |

### Preset mode state machine (main loop)

```
NORMAL
  └─ TAP long-press ≥1s ──► PRESET_BROWSE
                                MOD footswitch    → prev preset (slot-- then bank--, wraps)
                                REVERB footswitch → next preset (slot++ then bank++, wraps)
                                DELAY footswitch  → save live state to current slot
                                TAP short-press   → confirm + return to NORMAL
                                3s inactivity     → auto-exit (no change)

In PRESET_BROWSE:
  - Each MOD/REVERB press loads the new preset immediately (live audio preview)
  - All three FX LEDs blink at 2Hz
  - Display: "B3 · 07" on line 1, preset name on line 2
```

Advancing past slot 9 increments the bank; advancing past bank 9 wraps to bank 0. Same logic in reverse for MOD.

**Estimated scope:** ~400 lines new/changed across 6 files.

---

## Section 5: Tauri Desktop App

**Stack:** Tauri 2.x · Rust backend · React + TypeScript · Tailwind CSS · shadcn/ui · react-knob-headless

### Rust backend (`src-tauri/src/`)

| File | Responsibility |
|------|---------------|
| `midi.rs` | `midir` wrapper: port discovery, connect, send CC, send/receive SysEx |
| `sysex.rs` | 7-bit encode/decode, command frame builder, response parser |
| `commands.rs` | Tauri commands exposed to frontend |
| `main.rs` | Tauri app setup, state management |

**Tauri commands:**
`list_midi_ports`, `connect`, `disconnect`, `send_cc`, `get_preset`, `put_preset`, `get_all_presets`, `put_all_presets`, `set_active_preset`, `set_mode`

### Frontend layout

```
┌──────────────────────────────────────────────────────┐
│  MIDI port selector [shadcn Select]   [Connect] [Sync All] │
├───────────────┬───────────────┬──────────────────────┤
│  MOD          │  DELAY        │  REVERB              │
│  shadcn Card  │  shadcn Card  │  shadcn Card         │
│  mode Select  │  mode Select  │  mode Select         │
│  7 knobs      │  7 knobs      │  7 knobs             │
├───────────────┴───────────────┴──────────────────────┤
│  shadcn Tabs (Bank 0 … Bank 9)                        │
│  10 × shadcn Card preset slots per bank               │
│  [New] [Rename — shadcn Dialog] [Export .multifx]    │
└──────────────────────────────────────────────────────┘
```

**Knobs:** `react-knob-headless` (accessible, headless) styled with Tailwind to match shadcn. Each knob drag fires a CC message, throttled to one per 10ms.

**Mode selector:** shadcn `Select` per stage. On change, sends SysEx `SET_MODE`.

**Preset browser:** shadcn `Tabs` for bank, grid of shadcn `Card` for slots. Clicking a slot sends `SET_ACTIVE` SysEx. "Sync All" triggers `GET_ALL`. Export serialises all 100 presets to a `.multifx` JSON file; Import reads it and sends `PUT_ALL`.

### Frontend project structure

```
src/
├── App.tsx
├── components/
│   ├── StageCard.tsx          ← shadcn Card wrapping knobs + mode picker
│   ├── KnobPanel.tsx          ← react-knob-headless + Tailwind styling
│   ├── ModeSelector.tsx       ← shadcn Select
│   ├── PresetBrowser.tsx      ← shadcn Tabs + Card grid
│   └── ExportDialog.tsx       ← shadcn Dialog
└── hooks/
    └── useMidi.ts             ← wraps Tauri invoke calls
```

### `.multifx` file format

JSON envelope for backup/restore:

```json
{
  "version": 1,
  "device": "multi-fx",
  "banks": [
    {
      "bank": 0,
      "slots": [
        {
          "slot": 0,
          "name": "My Patch",
          "mod_mode": 0,
          "delay_mode": 2,
          "reverb_mode": 1,
          "mod_norm": [0.3, 0.5, 0.5, 0.5, 0.0, 0.0, 1.0],
          "delay_norm": [0.5, 0.4, 0.5, 0.5, 0.0, 0.0, 0.0],
          "reverb_norm": [0.4, 0.04, 0.5, 0.5, 0.0, 0.0, 0.5],
          "fx_enabled": [1, 1, 1]
        }
      ]
    }
  ]
}
```
