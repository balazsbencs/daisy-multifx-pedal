#pragma once
#include "daisy_seed.h"
#include "../config/constants.h"
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
};

class MidiHandlerPedal {
public:
    void Init(daisy::DaisySeed& hw);
    void Poll(MultiMidiState& out);

private:
    daisy::MidiUartHandler uart_;
    daisy::MidiUsbHandler  usb_;
    void ProcessEvent(daisy::MidiEvent e, MultiMidiState& out);
};

} // namespace pedal
