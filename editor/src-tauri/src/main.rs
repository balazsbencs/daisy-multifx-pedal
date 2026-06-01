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
        .setup(|app| {
            midi::watch_ports(app.handle().clone());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::list_midi_ports,
            commands::connect_midi,
            commands::send_cc,
            commands::get_preset,
            commands::put_preset,
            commands::get_all_presets,
            commands::set_active_preset,
            commands::set_mode,
            commands::set_fx_enabled,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
