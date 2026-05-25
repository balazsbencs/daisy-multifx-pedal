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
    if (!store_) return;
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
            // Sends 100 x 124-byte frames. USB TX FIFO (~512 bytes) will overflow;
            // desktop app should use 100 sequential GET_PRESET (0x01) calls instead
            // for reliable bulk sync.
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
        static_cast<uint8_t>(ok ? 0x00u : 0x01u),
        static_cast<uint8_t>(store_->GetActiveBank()),
        static_cast<uint8_t>(store_->GetActiveSlot()),
        0xF7u
    };
    usb_.SendMessage(ack, 8u);
}

} // namespace pedal
