use midir::{MidiInput, MidiOutput, MidiOutputConnection, Ignore};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Emitter};

pub struct MidiState {
    pub output: Option<MidiOutputConnection>,
    pub input_active: bool,
}

impl MidiState {
    pub fn new() -> Self {
        Self { output: None, input_active: false }
    }
}

pub fn list_ports() -> Vec<String> {
    let midi_out = match MidiOutput::new("multi-fx-list") {
        Ok(o) => o,
        Err(_) => return vec![],
    };
    midi_out.ports()
        .iter()
        .filter_map(|p| midi_out.port_name(p).ok())
        .collect()
}

pub fn connect(
    port_name: &str,
    state: Arc<Mutex<MidiState>>,
    app: AppHandle,
) -> Result<(), String> {
    // Output connection
    let midi_out = MidiOutput::new("multi-fx-out").map_err(|e| e.to_string())?;
    let out_port = midi_out
        .ports()
        .into_iter()
        .find(|p| midi_out.port_name(p).ok().as_deref() == Some(port_name))
        .ok_or_else(|| format!("port not found: {port_name}"))?;
    let conn = midi_out.connect(&out_port, "multi-fx-out").map_err(|e| e.to_string())?;

    {
        let mut s = state.lock().unwrap();
        s.output = Some(conn);
        s.input_active = true;
    }

    // Input connection (optional — no SysEx responses without it, but CC/mode/sysex
    // sending still works with output-only).
    let mut midi_in = match MidiInput::new("multi-fx-in") {
        Ok(mut m) => { m.ignore(Ignore::None); Some(m) }
        Err(_) => None,
    };
    let in_port = midi_in.as_ref().and_then(|m| {
        m.ports().into_iter().find(|p| m.port_name(p).ok().as_deref() == Some(port_name))
    });

    if let (Some(midi_in), Some(in_port)) = (midi_in.take(), in_port) {
        let state_clone = Arc::clone(&state);
        std::thread::spawn(move || {
            let _conn = midi_in.connect(
                &in_port,
                "multi-fx-in",
                move |_timestamp, message, _| {
                    handle_incoming(message, &app, &state_clone);
                },
                (),
            );
            // Block forever — the thread keeps the connection alive.
            loop { std::thread::sleep(std::time::Duration::from_secs(3600)); }
        });
    }

    Ok(())
}

fn handle_incoming(message: &[u8], app: &AppHandle, _state: &Arc<Mutex<MidiState>>) {
    if message.is_empty() { return; }
    // SysEx response: F0 7D <cmd> ...
    if message[0] == 0xF0 && message.len() >= 3 && message[1] == 0x7D {
        let _ = app.emit("midi-sysex", message.to_vec());
    }
}

/// Spawns a background thread that polls the MIDI output port list every 1.5 s.
/// Emits "midi-ports-changed" to the frontend whenever the list changes.
/// This handles devices that enumerate after the app starts, or that briefly
/// drop off and re-appear (e.g. Daisy Seed rebooting after a firmware flash).
pub fn watch_ports(app: AppHandle) {
    std::thread::spawn(move || {
        let mut last: Vec<String> = vec![];
        loop {
            let current = list_ports();
            if current != last {
                last = current.clone();
                let _ = app.emit("midi-ports-changed", current);
            }
            std::thread::sleep(std::time::Duration::from_millis(1500));
        }
    });
}

pub fn send_raw(state: &Arc<Mutex<MidiState>>, bytes: &[u8]) -> Result<(), String> {
    let mut s = state.lock().unwrap();
    match s.output.as_mut() {
        Some(conn) => conn.send(bytes).map_err(|e| e.to_string()),
        None => Err("not connected".into()),
    }
}
