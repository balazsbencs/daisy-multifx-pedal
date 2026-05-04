#include "midi_handler.h"

namespace pedal {

static constexpr float kCCScale = 1.0f / 127.0f;

void MidiHandlerPedal::Init(daisy::DaisySeed& hw) {
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
                out.mod_cc[i]    = val;
                out.mod_cc_rx[i] = true;
            } else if (cc >= CC_DELAY_BASE && cc < CC_DELAY_BASE + NUM_PARAMS) {
                const int i = cc - CC_DELAY_BASE;
                out.delay_cc[i]    = val;
                out.delay_cc_rx[i] = true;
            } else if (cc >= CC_REVERB_BASE && cc < CC_REVERB_BASE + NUM_PARAMS) {
                const int i = cc - CC_REVERB_BASE;
                out.reverb_cc[i]    = val;
                out.reverb_cc_rx[i] = true;
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
        default: break;
    }
}

} // namespace pedal
