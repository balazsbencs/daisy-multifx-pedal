use std::sync::{Arc, Mutex};
use tauri::State;
use crate::midi::{self, MidiState};
use crate::sysex;

type SharedMidi = Arc<Mutex<MidiState>>;

#[tauri::command]
pub fn list_midi_ports() -> Vec<String> {
    midi::list_ports()
}

#[tauri::command]
pub fn connect_midi(
    port_name: String,
    state: State<SharedMidi>,
    app: tauri::AppHandle,
) -> Result<(), String> {
    midi::connect(&port_name, Arc::clone(&state), app)
}

/// Send a MIDI CC message. channel is 0-indexed (0 = channel 1).
#[tauri::command]
pub fn send_cc(
    channel: u8,
    cc: u8,
    value: u8,
    state: State<SharedMidi>,
) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_cc(channel, cc, value))
}

#[tauri::command]
pub fn get_preset(bank: u8, slot: u8, state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_get_preset(bank, slot))
}

#[tauri::command]
pub fn put_preset(
    bank: u8,
    slot: u8,
    name: String,
    raw_data: Vec<u8>,
    state: State<SharedMidi>,
) -> Result<(), String> {
    if raw_data.len() != 92 {
        return Err(format!("raw_data must be 92 bytes, got {}", raw_data.len()));
    }
    let arr: &[u8; 92] = raw_data.as_slice().try_into().unwrap();
    midi::send_raw(&state, &sysex::build_put_preset(bank, slot, &name, arr))
}

#[tauri::command]
pub fn get_all_presets(state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_get_all())
}

#[tauri::command]
pub fn set_active_preset(bank: u8, slot: u8, state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_set_active(bank, slot))
}

#[tauri::command]
pub fn set_mode(stage: u8, mode_index: u8, state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_set_mode(stage, mode_index))
}

#[tauri::command]
pub fn set_fx_enabled(stage: u8, enabled: bool, state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_set_fx_enabled(stage, enabled))
}
