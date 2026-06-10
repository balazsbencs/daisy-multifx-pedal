# Preset System

The pedal stores up to **100 presets** organised into 10 banks of 10 slots
each (B0 P00 → B9 P09). Each preset captures the complete state of the
three effect stages: the selected algorithm, all seven parameters, and the
bypass on/off setting for each stage.

A separate **live state** slot is written automatically whenever you stop
tweaking, so your sound is restored exactly on the next power-up even if
you never explicitly save a preset.

---

## What a preset contains

| Field | Detail |
|-------|--------|
| Effect modes | Algorithm ID for MOD, DLY, and REV |
| Parameters | All 7 normalised params per stage (21 values total) |
| Bypass state | On/off for each of the three stages |

---

## Display

In normal operation the bottom of the display shows the **current bank
and slot** in a large white label:

```
B3 P07
```

`B` = bank number (0–9), `P` = slot number within that bank (00–09).

---

## Live state (auto-save)

The pedal continuously watches for parameter changes. **Two seconds after
the last edit** it writes the current sound to a dedicated live-state slot
in QSPI flash. On the next power-up that state is restored automatically —
modes, parameters, and bypass settings all come back exactly as you left
them.

You do not have to do anything for this to work. It is always on.

---

## Saving and loading presets

### Entering Browse mode

**Hold the TAP footswitch for 1 second.**

- All three footswitch LEDs start blinking at 2 Hz.
- The display shows **BROWSE** under the REV section of the chain strip.
- The bank/slot counter in the preset row shows your current position.

The pedal stays in Browse mode until you tap TAP to exit. There is no
automatic timeout.

### Navigating slots

| Footswitch | Action |
|------------|--------|
| **MOD** (left) | Previous slot |
| **REV** (right) | Next slot |

Navigation wraps: stepping back from B0 P00 goes to B9 P09, and forward
from B9 P09 goes to B0 P00. Bank rolls over automatically when you step
past slot 09 or before slot 00.

Each press takes effect immediately:

- The bank/slot counter on the display updates right away.
- If the destination slot contains a saved preset, it loads instantly —
  you hear the preset and the parameters update on screen.
- If the slot is empty the sound stays unchanged (nothing to load).
- The slot is also **set as the active slot** immediately, so the
  selection persists across power cycles without any extra confirmation.

### Saving the current sound to a slot

Navigate to the target slot, then press **DLY** (middle footswitch).

The current live sound — modes, parameters, and bypass state — is written
to that slot. An empty slot becomes valid; an existing preset is
overwritten.

### Exiting Browse mode

**Tap the TAP footswitch once.** LEDs return to their normal bypass
on/off state and the BROWSE indicator disappears.

---

## Summary of Browse mode controls

```
Hold TAP (≥ 1 s)    Enter Browse mode
MOD footswitch      Previous slot
REV footswitch      Next slot
DLY footswitch      Save current sound to displayed slot
TAP footswitch      Exit Browse mode
```

---

## MIDI control

For the complete wire-level protocol (exact byte frames, 7-bit encoding, live state broadcast) see [`MIDI_USB.md`](MIDI_USB.md).

### SysEx (USB MIDI)

The pedal responds to SysEx messages with manufacturer ID `0x7D`. All
replies are sent on USB MIDI.

| Command | Byte 1 | Description |
|---------|--------|-------------|
| `GET_PRESET` | `0x01` | Request the data for a specific bank/slot |
| `PUT_PRESET` | `0x02` | Write a full preset to a bank/slot and load it |
| `SET_ACTIVE` | `0x04` | Set active bank/slot and load it (no data transfer) |
| `GET_ALL`    | `0x05` | Dump all 100 presets sequentially |
| `GET_STATUS` | `0x06` | Query current active bank and slot |
| `SET_MODE`   | `0x07` | Change the algorithm on a stage |
| `SET_FX_ENABLED` | `0x08` | Toggle a stage bypass |

All commands reply with an ACK frame:

```
F0 7D 83 <cmd> <ok> <activeBank> <activeSlot> F7
```

`ok` = `0x00` for success, `0x01` for failure.

Preset data frames (`GET_PRESET` reply / `PUT_PRESET` payload) carry the
92-byte `MultiPresetSlot` struct encoded as 7-bit MIDI-safe bytes plus a
12-character name field.

### Desktop editor

The companion Tauri desktop app uses the SysEx protocol to sync all 100
presets over USB MIDI. It calls `GET_STATUS` on connect to identify the
currently loaded preset, then fetches individual presets on demand via
`GET_PRESET`.

---

## Storage details

Presets are stored in QSPI flash starting at offset `0x7E0000` from the
QSPI base address. The firmware does not touch this region during a normal
DFU firmware update, so presets survive firmware upgrades.

| Region | Offset | Content |
|--------|--------|---------|
| Header | `+0x000000` | Magic, version, active bank/slot, all 100 names |
| Data sectors | `+0x001000` | 100 preset slots (3 × 4 KB sectors) |
| Live state | `+0x004000` | 1 preset slot (1 × 4 KB sector) |

A slot is only loaded if its `valid` field equals exactly `1`. Erased flash
(`0xFF`) is treated as empty, so a fresh device starts with no presets and
the live sound is whatever the firmware initialises on first boot.
