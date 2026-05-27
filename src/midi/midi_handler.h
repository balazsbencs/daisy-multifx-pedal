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

    bool fx_enable_change = false; // SET_FX_ENABLED received
    int  fx_enable_stage  = 0;
    bool fx_enable_val    = false;
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
