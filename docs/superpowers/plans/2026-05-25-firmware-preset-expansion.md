# Firmware Preset Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand from 8 to 100 presets (10 banks × 10 slots) with a custom QSPI store, footswitch-based preset browse mode, and a USB MIDI SysEx handler for the desktop editor.

**Architecture:** Replace `MultiPresetManager`/`PersistentStorage` with a hand-rolled `QspiPresetStore` that writes directly to five dedicated QSPI sectors (20 KB at `0x007E0000`). Add a `HandleSysEx()` method to `MidiHandlerPedal` that responds to preset dump/load commands from the desktop app. Add a preset browse state machine in `main.cpp` driven by the four footswitches.

**Tech Stack:** C++17 · libDaisy QSPI HAL (`qspi.h`) · libDaisy MIDI USB (`midi.h`) · Daisy Seed STM32H7

> **Note:** There are no automated tests in this project. Each task verifies with `make -j4` (build) and, where noted, with a manual flash-and-verify step.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/config/constants.h` | Modify | Add `PRESET_BANK_COUNT`, `PRESET_SLOTS_PER_BANK`; update `PRESET_SLOT_COUNT` |
| `src/presets/preset_manager.h` | **Delete** | Replaced by `qspi_preset_store.h` |
| `src/presets/preset_manager.cpp` | **Delete** | Replaced by `qspi_preset_store.cpp` |
| `src/presets/qspi_preset_store.h` | **Create** | `MultiPresetSlot` struct + `QspiPresetStore` class declaration |
| `src/presets/qspi_preset_store.cpp` | **Create** | QSPI read/write/erase implementation |
| `src/midi/sysex_codec.h` | **Create** | `Encode7bit` / `Decode7bit` / `EncodedSize` / `DecodedSize` |
| `src/midi/sysex_codec.cpp` | **Create** | 7-bit encode/decode implementation |
| `src/midi/midi_handler.h` | Modify | Add SysEx fields to `MultiMidiState`; add `HandleSysEx`, `SendPresetData`, `SendAck`; `Init` gains `QspiPresetStore&` arg |
| `src/midi/midi_handler.cpp` | Modify | Implement SysEx dispatch + response sending |
| `src/main.cpp` | Modify | Replace `preset_manager` with `preset_store`; add bank/slot state; add preset browse state machine |

---

## Task 1: Update Constants

**Files:**
- Modify: `src/config/constants.h`

- [ ] **Step 1: Open `src/config/constants.h` and replace the presets block**

Replace:
```cpp
constexpr int      PRESET_SLOT_COUNT = 8;
constexpr uint32_t PRESET_HOLD_MS   = 700;
constexpr uint32_t PRESET_STATUS_MS = 1000;
```
With:
```cpp
constexpr int      PRESET_SLOT_COUNT     = 100;
constexpr int      PRESET_BANK_COUNT     = 10;
constexpr int      PRESET_SLOTS_PER_BANK = 10;
constexpr uint32_t PRESET_HOLD_MS        = 700;
constexpr uint32_t PRESET_STATUS_MS      = 1000;
```

- [ ] **Step 2: Build to confirm no new errors**

```bash
make -j4 2>&1 | tail -20
```
Expected: same errors as before (preset_manager still referenced), but no new parse errors.

- [ ] **Step 3: Commit**

```bash
git add src/config/constants.h
git commit -m "feat: expand PRESET_SLOT_COUNT to 100, add PRESET_BANK_COUNT/PRESET_SLOTS_PER_BANK"
```

---

## Task 2: Create QspiPresetStore Header

**Files:**
- Create: `src/presets/qspi_preset_store.h`

- [ ] **Step 1: Create the file with this exact content**

```cpp
#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"
#include <cstdint>
#include <cstring>

namespace pedal {

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
static_assert(sizeof(MultiPresetSlot) == 92,
    "MultiPresetSlot layout changed — update sysex encoding sizes if needed");

class QspiPresetStore {
public:
    void Init(daisy::QSPIHandle& qspi);

    int  GetActiveBank() const { return active_bank_; }
    int  GetActiveSlot() const { return active_slot_; }
    void SetActive(int bank, int slot);

    bool        LoadSlot(int bank, int slot, MultiPresetSlot& out) const;
    bool        SaveSlot(int bank, int slot, const MultiPresetSlot& data,
                         const char* name = nullptr);
    bool        LoadLiveState(MultiPresetSlot& out) const;
    bool        SaveLiveState(const MultiPresetSlot& data);
    const char* GetName(int bank, int slot) const;

private:
    static constexpr uint32_t kQspiBase     = 0x90000000u;
    static constexpr uint32_t kHeaderOffset = 0x007E0000u;
    static constexpr uint32_t kDataOffset   = 0x007E1000u; // 3 × 4 KB sectors
    static constexpr uint32_t kLiveOffset   = 0x007E4000u; // 1 × 4 KB sector
    static constexpr uint32_t kMagic        = 0x4D585053u; // "MXPS"
    static constexpr uint16_t kVersion      = 1u;
    static constexpr size_t   kSectorSize   = 4096u;

    daisy::QSPIHandle* qspi_        = nullptr;
    bool               initialized_ = false;
    int                active_bank_ = 0;
    int                active_slot_ = 0;
    char               names_[PRESET_SLOT_COUNT][12]{};

    int  LinearIndex(int bank, int slot) const {
        return bank * PRESET_SLOTS_PER_BANK + slot;
    }
    bool ValidIndex(int bank, int slot) const {
        return bank >= 0 && bank < PRESET_BANK_COUNT &&
               slot >= 0 && slot < PRESET_SLOTS_PER_BANK;
    }
    void ReadHeader();
    void WriteHeader();
};

} // namespace pedal
```

- [ ] **Step 2: Build to confirm header parses cleanly (build will still fail on preset_manager)**

```bash
make -j4 2>&1 | grep "qspi_preset_store" | head -5
```
Expected: no errors mentioning `qspi_preset_store.h`.

---

## Task 3: Implement QspiPresetStore

**Files:**
- Create: `src/presets/qspi_preset_store.cpp`

- [ ] **Step 1: Create the file**

```cpp
#include "qspi_preset_store.h"
#include <cstring>

namespace pedal {

// File-scope 4 KB buffer reused by SaveSlot to avoid stack usage.
static uint8_t s_sector_buf[4096];

struct PresetHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t  active_bank;
    uint8_t  active_slot;
    char     name[PRESET_SLOT_COUNT][12]; // 1200 bytes — fits in one 4 KB sector
};

bool MultiPresetSlot::operator!=(const MultiPresetSlot& o) const {
    return memcmp(this, &o, sizeof(*this)) != 0;
}

void QspiPresetStore::Init(daisy::QSPIHandle& qspi) {
    qspi_ = &qspi;
    ReadHeader();
    initialized_ = true;
}

void QspiPresetStore::ReadHeader() {
    const auto* h = reinterpret_cast<const PresetHeader*>(kQspiBase + kHeaderOffset);
    if (h->magic == kMagic && h->version == kVersion) {
        active_bank_ = h->active_bank;
        active_slot_ = h->active_slot;
        memcpy(names_, h->name, sizeof(names_));
    } else {
        // First boot / format mismatch — start fresh.
        active_bank_ = 0;
        active_slot_ = 0;
        memset(names_, 0, sizeof(names_));
    }
}

void QspiPresetStore::WriteHeader() {
    PresetHeader h{};
    h.magic       = kMagic;
    h.version     = kVersion;
    h.active_bank = static_cast<uint8_t>(active_bank_);
    h.active_slot = static_cast<uint8_t>(active_slot_);
    memcpy(h.name, names_, sizeof(names_));
    qspi_->EraseSector(kQspiBase + kHeaderOffset);
    qspi_->Write(kQspiBase + kHeaderOffset, sizeof(h),
                 reinterpret_cast<uint8_t*>(&h));
}

void QspiPresetStore::SetActive(int bank, int slot) {
    if (!ValidIndex(bank, slot)) return;
    active_bank_ = bank;
    active_slot_ = slot;
    WriteHeader();
}

bool QspiPresetStore::LoadSlot(int bank, int slot, MultiPresetSlot& out) const {
    if (!initialized_ || !ValidIndex(bank, slot)) return false;
    const uint32_t byte_offset = static_cast<uint32_t>(LinearIndex(bank, slot))
                                 * sizeof(MultiPresetSlot);
    const auto* ptr = reinterpret_cast<const MultiPresetSlot*>(
        kQspiBase + kDataOffset + byte_offset);
    if (!ptr->valid) return false;
    memcpy(&out, ptr, sizeof(out));
    return true;
}

bool QspiPresetStore::SaveSlot(int bank, int slot, const MultiPresetSlot& data,
                                const char* name) {
    if (!initialized_ || !ValidIndex(bank, slot)) return false;

    const uint32_t byte_offset      = static_cast<uint32_t>(LinearIndex(bank, slot))
                                      * sizeof(MultiPresetSlot);
    const uint32_t sector_base      = (byte_offset / kSectorSize) * kSectorSize;
    const uint32_t sector_addr      = kQspiBase + kDataOffset + sector_base;
    const uint32_t offset_in_sector = byte_offset - sector_base;

    // Read-modify-write: preserve all other presets in the same 4 KB sector.
    memcpy(s_sector_buf, reinterpret_cast<const void*>(sector_addr), kSectorSize);
    auto* dst = reinterpret_cast<MultiPresetSlot*>(s_sector_buf + offset_in_sector);
    *dst        = data;
    dst->valid  = 1;

    if (name) {
        const int idx = LinearIndex(bank, slot);
        strncpy(names_[idx], name, 11);
        names_[idx][11] = '\0';
    }

    qspi_->EraseSector(sector_addr);
    qspi_->Write(sector_addr, kSectorSize, s_sector_buf);
    WriteHeader(); // persists updated names and active bank/slot
    return true;
}

bool QspiPresetStore::LoadLiveState(MultiPresetSlot& out) const {
    if (!initialized_) return false;
    const auto* ptr = reinterpret_cast<const MultiPresetSlot*>(
        kQspiBase + kLiveOffset);
    if (!ptr->valid) return false;
    memcpy(&out, ptr, sizeof(out));
    return true;
}

bool QspiPresetStore::SaveLiveState(const MultiPresetSlot& data) {
    if (!initialized_) return false;
    static MultiPresetSlot buf;
    buf       = data;
    buf.valid = 1;
    qspi_->EraseSector(kQspiBase + kLiveOffset);
    qspi_->Write(kQspiBase + kLiveOffset, sizeof(buf),
                 reinterpret_cast<uint8_t*>(&buf));
    return true;
}

const char* QspiPresetStore::GetName(int bank, int slot) const {
    if (!ValidIndex(bank, slot)) return "";
    return names_[LinearIndex(bank, slot)];
}

} // namespace pedal
```

- [ ] **Step 2: Build (still expected to fail on preset_manager references)**

```bash
make -j4 2>&1 | grep "error:" | head -10
```
Expected: errors reference `preset_manager.h` in `main.cpp` only, not `qspi_preset_store`.

---

## Task 4: Create SysEx Codec

**Files:**
- Create: `src/midi/sysex_codec.h`
- Create: `src/midi/sysex_codec.cpp`

- [ ] **Step 1: Create `src/midi/sysex_codec.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace pedal {

// Every 7 binary bytes become 8 SysEx-safe bytes: one MSB byte + 7 data bytes.
// All output bytes have bit 7 clear, as required by MIDI SysEx.

size_t Encode7bit(const uint8_t* in, size_t in_len, uint8_t* out);
size_t Decode7bit(const uint8_t* in, size_t in_len, uint8_t* out);

// Exact output size when encoding in_len binary bytes.
// Each group of 7 bytes → 8 bytes; last partial group of k → k+1 bytes.
constexpr size_t EncodedSize(size_t in_len) {
    const size_t full = in_len / 7u;
    const size_t rem  = in_len % 7u;
    return full * 8u + (rem > 0u ? rem + 1u : 0u);
}

} // namespace pedal
```

- [ ] **Step 2: Create `src/midi/sysex_codec.cpp`**

```cpp
#include "sysex_codec.h"

namespace pedal {

size_t Encode7bit(const uint8_t* in, size_t in_len, uint8_t* out) {
    size_t out_idx = 0;
    size_t in_idx  = 0;
    while (in_idx < in_len) {
        const size_t chunk = (in_len - in_idx < 7u) ? (in_len - in_idx) : 7u;
        uint8_t msb = 0;
        for (size_t i = 0; i < chunk; ++i) {
            if (in[in_idx + i] & 0x80u) msb |= static_cast<uint8_t>(1u << i);
        }
        out[out_idx++] = msb;
        for (size_t i = 0; i < chunk; ++i) {
            out[out_idx++] = in[in_idx + i] & 0x7Fu;
        }
        in_idx += chunk;
    }
    return out_idx;
}

size_t Decode7bit(const uint8_t* in, size_t in_len, uint8_t* out) {
    size_t out_idx = 0;
    size_t in_idx  = 0;
    while (in_idx < in_len) {
        const size_t remaining = in_len - in_idx;
        if (remaining < 2u) break;
        const size_t chunk = ((remaining - 1u) < 7u) ? (remaining - 1u) : 7u;
        const uint8_t msb  = in[in_idx++];
        for (size_t i = 0; i < chunk; ++i) {
            out[out_idx++] = in[in_idx++] | (((msb >> i) & 1u) << 7);
        }
    }
    return out_idx;
}

} // namespace pedal
```

- [ ] **Step 3: Verify encode/decode round-trips with a quick Python sanity check on your development machine (not on the Daisy)**

```bash
python3 - <<'EOF'
import struct, random

def encode7(data):
    out = []
    for i in range(0, len(data), 7):
        chunk = data[i:i+7]
        msb = sum((1 << j) for j, b in enumerate(chunk) if b & 0x80)
        out.append(msb)
        out.extend(b & 0x7F for b in chunk)
    return bytes(out)

def decode7(data):
    out = []
    i = 0
    while i < len(data):
        remaining = len(data) - i
        if remaining < 2: break
        chunk_len = min(remaining - 1, 7)
        msb = data[i]; i += 1
        for j in range(chunk_len):
            out.append(data[i] | (((msb >> j) & 1) << 7)); i += 1
    return bytes(out)

# Test with 92 random bytes (one MultiPresetSlot worth of floats)
original = bytes(random.randint(0, 255) for _ in range(92))
encoded  = encode7(original)
decoded  = decode7(encoded)
assert decoded == original, "round-trip failed!"
n = len(original)
expected = (n // 7) * 8 + (n % 7 + 1 if n % 7 > 0 else 0)
assert len(encoded) == expected, f"size wrong: {len(encoded)} != {expected}"
print(f"OK: 92 bytes → {len(encoded)} encoded → {len(decoded)} decoded, round-trip matches")
EOF
```
Expected output: `OK: 92 bytes → 106 encoded → 92 decoded, round-trip matches`

- [ ] **Step 4: Commit**

```bash
git add src/midi/sysex_codec.h src/midi/sysex_codec.cpp
git commit -m "feat: add MIDI SysEx 7-bit encode/decode codec"
```

---

## Task 5: Update MidiHandler for SysEx

**Files:**
- Modify: `src/midi/midi_handler.h`
- Modify: `src/midi/midi_handler.cpp`

- [ ] **Step 1: Replace `src/midi/midi_handler.h` with this content**

```cpp
#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"
#include "../presets/qspi_preset_store.h"
#include "sysex_codec.h"
#include <cstdint>

namespace pedal {

struct MultiMidiState {
    float mod_cc[NUM_PARAMS]{};    bool mod_cc_rx[NUM_PARAMS]{};
    float delay_cc[NUM_PARAMS]{};  bool delay_cc_rx[NUM_PARAMS]{};
    float reverb_cc[NUM_PARAMS]{}; bool reverb_cc_rx[NUM_PARAMS]{};

    int  program_change = -1;
    bool clock_tick     = false;
    bool clock_stop     = false;
    bool hold_on        = false;
    bool hold_off       = false;

    // SysEx-driven actions (set by HandleSysEx, acted on in main loop)
    bool preset_load = false; // SET_ACTIVE or PUT_PRESET received
    int  sysex_bank  = 0;
    int  sysex_slot  = 0;
    bool mode_change = false; // SET_MODE received
    int  mode_stage  = 0;     // 0=mod 1=delay 2=reverb
    int  mode_index  = 0;
};

class MidiHandlerPedal {
public:
    // store must outlive this object.
    void Init(daisy::DaisySeed& hw, QspiPresetStore& store);
    void Poll(MultiMidiState& out);

private:
    daisy::MidiUartHandler uart_;
    daisy::MidiUsbHandler  usb_;
    QspiPresetStore*       store_ = nullptr;

    void ProcessEvent(daisy::MidiEvent e, MultiMidiState& out);
    void HandleSysEx(const uint8_t* data, size_t len, MultiMidiState& out);
    void SendPresetData(int bank, int slot);
    void SendAck(uint8_t cmd, bool ok);
};

} // namespace pedal
```

- [ ] **Step 2: Replace `src/midi/midi_handler.cpp` with this content**

```cpp
#include "midi_handler.h"
#include <cstring>

namespace pedal {

static constexpr float kCCScale = 1.0f / 127.0f;

void MidiHandlerPedal::Init(daisy::DaisySeed& hw, QspiPresetStore& store) {
    store_ = &store;
    daisy::MidiUartHandler::Config uart_cfg;
    uart_.Init(uart_cfg);
    daisy::MidiUsbHandler::Config usb_cfg;
    usb_.Init(usb_cfg);
}

void MidiHandlerPedal::Poll(MultiMidiState& out) {
    out = MultiMidiState{};
    uart_.Listen();
    while (uart_.HasEvents()) ProcessEvent(uart_.PopEvent(), out);
    usb_.Listen();
    while (usb_.HasEvents()) ProcessEvent(usb_.PopEvent(), out);
}

void MidiHandlerPedal::ProcessEvent(daisy::MidiEvent e, MultiMidiState& out) {
    switch (e.type) {
        case daisy::ControlChange: {
            const uint8_t cc  = e.data[0];
            const float   val = static_cast<float>(e.data[1]) * kCCScale;
            if (cc >= CC_MOD_BASE && cc < CC_MOD_BASE + NUM_PARAMS) {
                const int i = cc - CC_MOD_BASE;
                out.mod_cc[i] = val; out.mod_cc_rx[i] = true;
            } else if (cc >= CC_DELAY_BASE && cc < CC_DELAY_BASE + NUM_PARAMS) {
                const int i = cc - CC_DELAY_BASE;
                out.delay_cc[i] = val; out.delay_cc_rx[i] = true;
            } else if (cc >= CC_REVERB_BASE && cc < CC_REVERB_BASE + NUM_PARAMS) {
                const int i = cc - CC_REVERB_BASE;
                out.reverb_cc[i] = val; out.reverb_cc_rx[i] = true;
            } else if (cc == CC_HOLD) {
                if (val > 0.5f) out.hold_on  = true;
                else            out.hold_off = true;
            }
            break;
        }
        case daisy::ProgramChange:
            out.program_change = static_cast<int>(e.data[0]);
            break;
        case daisy::SystemRealTime:
            if (e.data[0] == 0xF8) out.clock_tick = true;
            if (e.data[0] == 0xFC) out.clock_stop = true;
            break;
        case daisy::SystemExclusive:
            // sysex_data contains bytes between F0 and F7 (exclusive).
            // data[0] = manufacturer ID (0x7D), data[1] = command.
            HandleSysEx(e.sysex_data, e.sysex_message_len, out);
            break;
        default: break;
    }
}

void MidiHandlerPedal::HandleSysEx(const uint8_t* data, size_t len,
                                    MultiMidiState& out) {
    if (len < 2 || data[0] != 0x7Du) return; // wrong manufacturer ID
    const uint8_t cmd = data[1];

    switch (cmd) {
        case 0x01u: { // GET_PRESET
            if (len < 4u) { SendAck(cmd, false); return; }
            SendPresetData(static_cast<int>(data[2]), static_cast<int>(data[3]));
            break;
        }
        case 0x02u: { // PUT_PRESET
            // payload: bank(1) slot(1) name[12] encoded_data[EncodedSize(92)]
            constexpr size_t kEncLen = EncodedSize(92);
            if (len < 2u + 2u + 12u + kEncLen) { SendAck(cmd, false); return; }
            const int   bank = static_cast<int>(data[2]);
            const int   slot = static_cast<int>(data[3]);
            char        name[12];
            memcpy(name, data + 4u, 12u);
            name[11] = '\0';
            uint8_t raw[92];
            Decode7bit(data + 4u + 12u, kEncLen, raw);
            MultiPresetSlot s;
            memcpy(&s, raw, sizeof(s));
            store_->SaveSlot(bank, slot, s, name);
            out.preset_load = true;
            out.sysex_bank  = bank;
            out.sysex_slot  = slot;
            SendAck(cmd, true);
            break;
        }
        case 0x04u: { // SET_ACTIVE
            if (len < 4u) { SendAck(cmd, false); return; }
            const int bank = static_cast<int>(data[2]);
            const int slot = static_cast<int>(data[3]);
            store_->SetActive(bank, slot);
            out.preset_load = true;
            out.sysex_bank  = bank;
            out.sysex_slot  = slot;
            SendAck(cmd, true);
            break;
        }
        case 0x05u: { // GET_ALL
            for (int b = 0; b < PRESET_BANK_COUNT; ++b)
                for (int s = 0; s < PRESET_SLOTS_PER_BANK; ++s)
                    SendPresetData(b, s);
            break;
        }
        case 0x07u: { // SET_MODE
            if (len < 4u) { SendAck(cmd, false); return; }
            out.mode_change = true;
            out.mode_stage  = static_cast<int>(data[2]);
            out.mode_index  = static_cast<int>(data[3]);
            SendAck(cmd, true);
            break;
        }
        default:
            SendAck(cmd, false);
            break;
    }
}

void MidiHandlerPedal::SendPresetData(int bank, int slot) {
    if (!store_) return;
    MultiPresetSlot s{};
    store_->LoadSlot(bank, slot, s);
    const char* name = store_->GetName(bank, slot);

    // Frame: F0 7D 81 bank slot name[12] encoded[106] F7  (total ≤ 128 bytes)
    constexpr size_t kEncLen = EncodedSize(92);
    static uint8_t buf[1u + 1u + 1u + 1u + 1u + 12u + kEncLen + 1u];
    size_t i = 0;
    buf[i++] = 0xF0u;
    buf[i++] = 0x7Du;
    buf[i++] = 0x81u;
    buf[i++] = static_cast<uint8_t>(bank);
    buf[i++] = static_cast<uint8_t>(slot);
    memcpy(buf + i, name, 12u); i += 12u;
    uint8_t raw[92];
    memcpy(raw, &s, sizeof(s));
    i += Encode7bit(raw, sizeof(raw), buf + i);
    buf[i++] = 0xF7u;
    usb_.SendMessage(buf, i);
}

void MidiHandlerPedal::SendAck(uint8_t cmd, bool ok) {
    if (!store_) return;
    uint8_t ack[8] = {
        0xF0u, 0x7Du, 0x83u,
        cmd,
        ok ? 0x00u : 0x01u,
        static_cast<uint8_t>(store_->GetActiveBank()),
        static_cast<uint8_t>(store_->GetActiveSlot()),
        0xF7u
    };
    usb_.SendMessage(ack, 8u);
}

} // namespace pedal
```

- [ ] **Step 3: Build — should now only fail on main.cpp references to preset_manager**

```bash
make -j4 2>&1 | grep "error:" | head -10
```
Expected: errors only in `main.cpp` about `MultiPresetManager` / `preset_manager`.

---

## Task 6: Wire QspiPresetStore into main.cpp

**Files:**
- Modify: `src/main.cpp`
- Delete: `src/presets/preset_manager.h`, `src/presets/preset_manager.cpp`

- [ ] **Step 1: In `src/main.cpp`, replace the preset_manager include and declaration**

Replace:
```cpp
#include "presets/preset_manager.h"
```
With:
```cpp
#include "presets/qspi_preset_store.h"
```

Replace:
```cpp
static MultiPresetManager preset_manager;
```
With:
```cpp
static QspiPresetStore preset_store;
```

Replace:
```cpp
static int  preset_slot   = 0;
```
With:
```cpp
static int  preset_bank   = 0;
static int  preset_slot   = 0;
```

- [ ] **Step 2: Replace `preset_manager.Init(hw)` in main()**

Replace:
```cpp
preset_manager.Init(hw);
```
With:
```cpp
preset_store.Init(hw.qspi);
```

Also update the `midi_handler.Init` call — it now requires the store:
```cpp
midi_handler.Init(hw, preset_store);
```

- [ ] **Step 3: Replace live state calls**

Replace:
```cpp
if (preset_manager.LoadLiveState(live) &&
```
With:
```cpp
if (preset_store.LoadLiveState(live) &&
```

Replace:
```cpp
preset_manager.SaveLiveState(SnapshotLiveState());
```
With:
```cpp
preset_store.SaveLiveState(SnapshotLiveState());
```

- [ ] **Step 4: Update display call — replace old `preset_slot` arg with bank·slot combined index**

Find the `display.Update(...)` call and update the `preset_slot` argument:
```cpp
display.Update(active_page, shift,
               cur_mod, cur_delay, cur_reverb,
               buf.mod, buf.delay, buf.reverb,
               fx_enabled, hold_active,
               preset_bank * PRESET_SLOTS_PER_BANK + preset_slot,
               PresetUiEvent::None, now);
```

- [ ] **Step 5: Delete old preset_manager files**

```bash
git rm src/presets/preset_manager.h src/presets/preset_manager.cpp
```

- [ ] **Step 6: Build — should compile cleanly**

```bash
make -j4 2>&1 | tail -5
```
Expected: `Linking build/multi-fx.elf` and no errors.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp src/presets/qspi_preset_store.h src/presets/qspi_preset_store.cpp \
        src/midi/midi_handler.h src/midi/midi_handler.cpp
git commit -m "feat: replace MultiPresetManager with QspiPresetStore (100-preset QSPI layout)"
```

---

## Task 7: Preset Browse State Machine in main.cpp

**Files:**
- Modify: `src/main.cpp`

This task adds the footswitch-driven preset browsing mode. The tap footswitch currently handles tap tempo (short press) and reverb hold (held >500 ms). Preset mode entry uses held ≥1000 ms; once that threshold is crossed, reverb hold is suppressed.

- [ ] **Step 1: Add browse state variables after existing main-loop statics**

After `static uint32_t last_enc_tick_ms[NUM_PARAMS]{};`, add:

```cpp
// Preset browse mode
enum class PresetMode { Normal, Browse };
static PresetMode preset_mode     = PresetMode::Normal;
static int  browse_bank           = 0;
static int  browse_slot           = 0;
static bool tap_preset_pending    = false; // true once tap held ≥1000 ms
static uint32_t tap_hold_start_ms = 0;
static uint32_t last_browse_ms    = 0;
static constexpr uint32_t kPresetEntryHoldMs     = 1000u;
static constexpr uint32_t kPresetInactivityMs    = 3000u;
static constexpr uint32_t kPresetLedBlinkPeriodMs = 500u;
```

- [ ] **Step 2: Add a LoadPreset helper after SnapshotLiveState()**

```cpp
static void LoadPreset(int bank, int slot) {
    MultiPresetSlot p{};
    if (!preset_store.LoadSlot(bank, slot, p)) return;
    if (p.mod_mode    < static_cast<uint8_t>(NUM_MOD_MODES))    SwitchModMode(static_cast<ModModeId>(p.mod_mode));
    if (p.delay_mode  < static_cast<uint8_t>(NUM_DELAY_MODES))  SwitchDelayMode(static_cast<DelayModeId>(p.delay_mode));
    if (p.reverb_mode < static_cast<uint8_t>(NUM_REVERB_MODES)) SwitchReverbMode(static_cast<ReverbModeId>(p.reverb_mode));
    for (int i = 0; i < NUM_PARAMS; ++i) {
        mod_norm[i]    = Clamp01(p.mod_norm[i]);
        delay_norm[i]  = Clamp01(p.delay_norm[i]);
        reverb_norm[i] = Clamp01(p.reverb_norm[i]);
    }
    for (int i = 0; i < 3; ++i) {
        fx_enabled[i] = p.fx_enabled[i];
        led_fx[i].Write(fx_enabled[i]);
    }
    preset_bank = bank;
    preset_slot = slot;
}
```

- [ ] **Step 3: Replace the tap footswitch block in the main loop**

Find this block:
```cpp
        // ── Tap footswitch ─────────────────────────────────────────────────────
        if (ctrl.tap_pressed) {
            tempo_sync.OnTap(now);
        }
        if (ctrl.tap_held && ctrl.tap_held_ms > 500) {
            if (!hold_active) {
                hold_active = true;
                audio_engine.SetHold(true);
            }
        }
        if (ctrl.tap_released && ctrl.tap_held_ms <= 500) {
            if (hold_active) {
                hold_active = false;
                audio_engine.SetHold(false);
            }
        }
```

Replace with:

```cpp
        // ── Tap footswitch ─────────────────────────────────────────────────────
        if (preset_mode == PresetMode::Normal) {
            if (ctrl.tap_pressed) {
                tap_hold_start_ms    = now;
                tap_preset_pending   = false;
                tempo_sync.OnTap(now);
            }
            if (ctrl.tap_held && !tap_preset_pending &&
                (now - tap_hold_start_ms) >= kPresetEntryHoldMs) {
                tap_preset_pending = true;
                // Suppress reverb hold — preset mode takes priority.
                if (hold_active) { hold_active = false; audio_engine.SetHold(false); }
            }
            if (!tap_preset_pending && ctrl.tap_held &&
                ctrl.tap_held_ms > 500 && !hold_active) {
                hold_active = true;
                audio_engine.SetHold(true);
            }
            if (ctrl.tap_released) {
                if (tap_preset_pending) {
                    // Enter browse mode at the bank/slot that is currently active.
                    browse_bank        = preset_bank;
                    browse_slot        = preset_slot;
                    preset_mode        = PresetMode::Browse;
                    last_browse_ms     = now;
                    tap_preset_pending = false;
                } else if (hold_active && ctrl.tap_held_ms <= 500) {
                    hold_active = false;
                    audio_engine.SetHold(false);
                }
            }
        } else { // PresetMode::Browse
            if (ctrl.tap_pressed) {
                // Confirm selection and exit.
                preset_store.SetActive(browse_bank, browse_slot);
                preset_bank  = browse_bank;
                preset_slot  = browse_slot;
                preset_mode  = PresetMode::Normal;
                for (int i = 0; i < 3; ++i) led_fx[i].Write(fx_enabled[i]);
            }
        }
```

- [ ] **Step 4: Add browse footswitch handling inside the fx footswitch block**

Replace the existing fx footswitch block:
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

Replace with:
```cpp
        // ── Effect on/off footswitches (normal) / browse nav (preset mode) ────
        if (preset_mode == PresetMode::Normal) {
            for (int i = 0; i < 3; ++i) {
                if (ctrl.fx_pressed[i]) {
                    fx_enabled[i]    = !fx_enabled[i];
                    led_fx[i].Write(fx_enabled[i]);
                    live_state_dirty = true;
                    last_change_ms   = now;
                }
            }
        } else { // PresetMode::Browse
            // fx[0]=MOD → prev, fx[1]=DELAY → save here, fx[2]=REVERB → next
            if (ctrl.fx_pressed[0]) { // prev preset
                if (--browse_slot < 0) {
                    browse_slot = PRESET_SLOTS_PER_BANK - 1;
                    browse_bank = (browse_bank - 1 + PRESET_BANK_COUNT) % PRESET_BANK_COUNT;
                }
                LoadPreset(browse_bank, browse_slot);
                last_browse_ms = now;
            }
            if (ctrl.fx_pressed[2]) { // next preset
                if (++browse_slot >= PRESET_SLOTS_PER_BANK) {
                    browse_slot = 0;
                    browse_bank = (browse_bank + 1) % PRESET_BANK_COUNT;
                }
                LoadPreset(browse_bank, browse_slot);
                last_browse_ms = now;
            }
            if (ctrl.fx_pressed[1]) { // save live state to this slot
                const MultiPresetSlot snap = SnapshotLiveState();
                preset_store.SaveSlot(browse_bank, browse_slot, snap);
                last_browse_ms = now;
            }

            // Blink all three LEDs at 2 Hz while browsing.
            const bool blink_on = ((now % kPresetLedBlinkPeriodMs) < (kPresetLedBlinkPeriodMs / 2));
            for (int i = 0; i < 3; ++i) led_fx[i].Write(blink_on);

            // Auto-exit after inactivity.
            if ((now - last_browse_ms) >= kPresetInactivityMs) {
                // Revert to previously confirmed preset.
                LoadPreset(preset_store.GetActiveBank(), preset_store.GetActiveSlot());
                preset_mode = PresetMode::Normal;
                for (int i = 0; i < 3; ++i) led_fx[i].Write(fx_enabled[i]);
            }
        }
```

- [ ] **Step 5: Handle SysEx preset_load and mode_change from main loop MIDI block**

In the MIDI processing block, after the existing CC/hold/clock handling, add:

```cpp
        if (midi.preset_load) {
            LoadPreset(midi.sysex_bank, midi.sysex_slot);
            live_state_dirty = true;
            last_change_ms   = now;
        }
        if (midi.mode_change) {
            const int idx = midi.mode_index;
            switch (midi.mode_stage) {
                case 0: if (idx >= 0 && idx < NUM_MOD_MODES)    SwitchModMode(static_cast<ModModeId>(idx));    break;
                case 1: if (idx >= 0 && idx < NUM_DELAY_MODES)  SwitchDelayMode(static_cast<DelayModeId>(idx));  break;
                case 2: if (idx >= 0 && idx < NUM_REVERB_MODES) SwitchReverbMode(static_cast<ReverbModeId>(idx)); break;
            }
            live_state_dirty = true;
            last_change_ms   = now;
        }
```

- [ ] **Step 6: Build**

```bash
make -j4 2>&1 | tail -5
```
Expected: clean build with no errors.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add 10×10 bank/slot nav and footswitch preset browse mode"
```

---

## Task 8: Flash and Hardware Verify

- [ ] **Step 1: Flash firmware**

Put the device in DFU mode (press RESET only → Daisy Bootloader prompt within 2s):
```bash
make program-dfu
```

- [ ] **Step 2: Verify live state persists across power cycle**
  1. Change a parameter (turn any encoder)
  2. Wait 2 seconds (auto-save debounce)
  3. Power cycle the device
  4. Confirm the parameter is restored

- [ ] **Step 3: Verify preset browse mode**
  1. Hold TAP for ≥1s → all three LEDs start blinking
  2. Press MOD footswitch → display shows B0·09 (or wraps correctly)
  3. Press REVERB footswitch → increments slot/bank
  4. Press TAP → LEDs return to normal, selection confirmed

- [ ] **Step 4: Verify SysEx via MIDI monitor (optional, requires desktop or MIDI tool)**

Using a MIDI tool (e.g., MIDI Monitor on macOS), send:
```
F0 7D 01 00 00 F7
```
Expected response: `F0 7D 81 00 00 [12 name bytes] [106 encoded bytes] F7`

- [ ] **Step 5: Final commit if any fixes were needed**

```bash
git add -p
git commit -m "fix: hardware verification corrections"
```
