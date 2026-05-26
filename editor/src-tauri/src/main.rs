#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;
mod midi;
mod sysex;

use std::sync::{Arc, Mutex};
use midi::MidiState;

fn main() {
    let midi_state = Arc::new(Mutex::new(MidiState::new()));

    tauri::Builder::default()
        .manage(midi_state)
        .invoke_handler(tauri::generate_handler![])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
