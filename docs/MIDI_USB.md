# MIDI over USB

The pedal exposes a class-compliant USB MIDI device. No driver is needed on macOS/Windows/Linux. UART MIDI (TRS/DIN) is processed by the same handler and supports all the same incoming messages; **SysEx responses are sent over USB only**.

---

## Transport

`MidiHandlerPedal` (in `src/midi/midi_handler.h`) wraps libDaisy's `MidiUartHandler` and `MidiUsbHandler`. Both are polled in the main loop on every iteration (~1 kHz):

```
midi_handler.Poll(midi);
  ├─ uart_.Listen() → drain UART ring buffer
  └─ usb_.Listen()  → drain USB MIDI FIFO
```

All received events are normalised into a `MultiMidiState` struct and acted on synchronously before the next audio-parameter update.

---

## MIDI Channel

The handler processes events on **all channels** (channel-filtering is not implemented).

---

## Control Change (CC)

Parameters arrive as 7-bit values (0–127) and are normalised to `[0.0, 1.0]` before being written to the parameter arrays.

| CC numbers | Stage       | Parameters (index 0–6)                                 |
|------------|-------------|--------------------------------------------------------|
| 14–20      | Modulation  | Speed, Depth, Mix, Tone, P1, P2, Level                 |
| 21–27      | Delay       | Time, Repeats, Mix, Filter, Grit, Mod Speed, Mod Depth |
| 28–34      | Reverb      | Decay, Pre-delay, Mix, Tone, Mod, Param1, Param2       |
| 65         | Reverb hold | >63 → hold on, ≤63 → hold off                         |

Each received CC immediately updates the corresponding normalised value and sets `live_state_dirty`, which triggers a debounced live-state broadcast back to the editor (see [Live State](#live-state)).

---

## Program Change

A Program Change message activates preset slot `PC value` in the current bank (bank 0 if none has been explicitly set via SysEx). The preset is loaded immediately into the engine.

---

## MIDI Clock

MIDI Clock messages (0xF8 / System Real-Time) are forwarded to `TempoSync::OnMidiClock()`. The tempo sync module derives a beat period from the clock and locks the delay time and modulation speed when the clock is running. A Stop message (0xFC) releases the lock and reverts parameters to encoder/CC control.

See `src/tempo/tempo_sync.h` for the priority chain (MIDI Clock > Tap Tempo > Encoder).

---

## SysEx Protocol

Manufacturer ID: **`0x7D`** (private / research use)

### General frame format

```
F0  7D  <cmd>  [payload…]  F7
```

All multi-byte binary payloads are [7-bit encoded](#7-bit-encoding) so every byte stays below 0x80.

For the user-facing preset workflow (browsing, saving from the front panel, storage layout) see [`PRESETS.md`](PRESETS.md).

### Commands received by the pedal

| Cmd  | Name           | Payload                                     | Description |
|------|----------------|---------------------------------------------|-------------|
| 0x01 | GET_PRESET     | `bank(1) slot(1)`                           | Pedal replies with a PRESET_DATA frame (0x81). |
| 0x02 | PUT_PRESET     | `bank(1) slot(1) name[12] encoded[106]`     | Writes preset to QSPI, activates it, replies ACK. |
| 0x04 | SET_ACTIVE     | `bank(1) slot(1)`                           | Activates a previously stored preset, replies ACK. |
| 0x05 | GET_ALL        | *(none)*                                    | Sends 100 sequential PRESET_DATA frames. **Unreliable** — USB TX FIFO (~512 bytes) overflows; use 100 sequential GET_PRESET calls instead. |
| 0x06 | GET_STATUS     | *(none)*                                    | Replies ACK containing the current active bank/slot. |
| 0x07 | SET_MODE       | `stage(1) mode_index(1)`                   | Switches effect mode. stage: 0=mod, 1=delay, 2=reverb. Replies ACK. |
| 0x08 | SET_FX_ENABLED | `stage(1) enabled(1)`                      | Bypasses (0) or enables (non-zero) a stage. Replies ACK. |
| 0x0B | GET_LIVE_STATE | *(none)*                                    | Pedal replies immediately with a LIVE_STATE frame (0x82). |

### Frames sent by the pedal

#### 0x81 — PRESET_DATA

Sent in response to GET_PRESET (0x01) or GET_ALL (0x05).

```
F0  7D  81  bank  slot  name[12]  encoded_preset[106]  F7
```

Total frame: 122 bytes. `encoded_preset` is the 7-bit encoding of the 92-byte `MultiPresetSlot` struct.

#### 0x82 — LIVE_STATE

Sent in response to GET_LIVE_STATE (0x0B) and also broadcast automatically after any encoder or CC change (debounced).

```
F0  7D  82  bank  slot  encoded_state[106]  F7
```

Total frame: 112 bytes. `encoded_state` is the 7-bit encoding of the current live `MultiPresetSlot` snapshot (modes + all 21 normalised parameters + bypass flags).

#### 0x83 — ACK

Sent after every command that modifies state.

```
F0  7D  83  cmd  status  active_bank  active_slot  F7
```

| Byte          | Value |
|---------------|-------|
| `cmd`         | Echo of the command byte being acknowledged |
| `status`      | 0x00 = OK, 0x01 = error |
| `active_bank` | Currently active preset bank |
| `active_slot` | Currently active preset slot |

---

## Live State

The pedal maintains a **live state** — the current set of normalised parameters, modes, and bypass flags — distinct from the saved presets.

Two conditions trigger a broadcast of `LIVE_STATE` (0x82) over USB:

1. **On request** — `GET_LIVE_STATE` (0x0B) causes an immediate reply.
2. **Auto-broadcast** — any encoder turn, footswitch press, or CC received sets `hw_live_state_dirty`. After a short debounce (~200 ms), the pedal sends the current snapshot automatically. This keeps the desktop editor in sync without polling.

Live state changes (encoder/CC) are also written back to QSPI flash after their own debounce (~1 s) via `preset_store.SaveLiveState()`, so they survive a power cycle.

---

## 7-bit Encoding

MIDI SysEx data bytes must have bit 7 clear. Binary structs are encoded as follows:

```
Input:  [ B0 B1 B2 B3 B4 B5 B6 ]  (7 bytes)
Output: [ msb D0 D1 D2 D3 D4 D5 D6 ]  (8 bytes)
```

- `msb`: each bit `i` is the original bit 7 of `B[i]`
- `D[i]`: `B[i] & 0x7F`

The last partial group of `k < 7` bytes maps to `k + 1` output bytes using the same scheme. The exact encoded length for `n` input bytes is:

```
full_groups * 8 + (remainder > 0 ? remainder + 1 : 0)
```

For the 92-byte `MultiPresetSlot` struct this produces **106 bytes** (13 full groups of 7 → 104 bytes, plus 1 remaining → 2 bytes).

See `src/midi/sysex_codec.h` and `sysex_codec.cpp` for `Encode7bit` / `Decode7bit`.

---

## Source Files

| File | Purpose |
|------|---------|
| `src/midi/midi_handler.h`   | `MidiHandlerPedal` declaration, `MultiMidiState` struct |
| `src/midi/midi_handler.cpp` | UART + USB polling, CC/PC/clock dispatch, SysEx command handling |
| `src/midi/sysex_codec.h`    | `Encode7bit`, `Decode7bit`, `EncodedSize` |
| `src/midi/sysex_codec.cpp`  | 7-bit codec implementation |
| `src/config/constants.h`    | `CC_MOD_BASE`, `CC_DELAY_BASE`, `CC_REVERB_BASE`, `CC_HOLD` |
| `src/main.cpp`              | Main loop consumes `MultiMidiState`, calls `SendLiveState` |
