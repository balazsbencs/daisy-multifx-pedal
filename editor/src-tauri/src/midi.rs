use midir::{Ignore, MidiInput, MidiOutput, MidiOutputConnection};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Emitter};

pub struct MidiState {
    pub output: Option<MidiOutputConnection>,
    pub input_active: bool,
}

impl MidiState {
    pub fn new() -> Self {
        Self {
            output: None,
            input_active: false,
        }
    }
}

pub fn list_ports() -> Vec<String> {
    let Ok(midi_out) = MidiOutput::new("multi-fx-list") else {
        return Vec::new();
    };

    midi_out
        .ports()
        .iter()
        .filter_map(|port| midi_out.port_name(port).ok())
        .collect()
}

pub fn connect(
    port_name: &str,
    state: Arc<Mutex<MidiState>>,
    app: AppHandle,
) -> Result<(), String> {
    let midi_out = MidiOutput::new("multi-fx-out").map_err(|err| err.to_string())?;
    let out_ports = midi_out.ports();
    let out_port = out_ports
        .iter()
        .find(|port| midi_out.port_name(port).ok().as_deref() == Some(port_name))
        .ok_or_else(|| format!("output port not found: {port_name}"))?;
    let conn = midi_out
        .connect(out_port, "multi-fx-out")
        .map_err(|err| err.to_string())?;

    {
        let mut locked = state.lock().map_err(|err| err.to_string())?;
        locked.output = Some(conn);
        locked.input_active = true;
    }

    let mut midi_in = MidiInput::new("multi-fx-in").map_err(|err| err.to_string())?;
    midi_in.ignore(Ignore::None);
    let in_ports = midi_in.ports();
    let in_port = in_ports
        .iter()
        .find(|port| midi_in.port_name(port).ok().as_deref() == Some(port_name))
        .ok_or_else(|| format!("input port not found: {port_name}"))?
        .clone();

    std::thread::spawn(move || {
        let Ok(_conn) = midi_in.connect(
            &in_port,
            "multi-fx-in",
            move |_timestamp, message, _| {
                handle_incoming(message, &app);
            },
            (),
        ) else {
            return;
        };

        loop {
            std::thread::sleep(std::time::Duration::from_secs(3600));
        }
    });

    Ok(())
}

fn handle_incoming(message: &[u8], app: &AppHandle) {
    if message.len() >= 3 && message[0] == 0xF0 && message[1] == 0x7D {
        let _ = app.emit("midi-sysex", message.to_vec());
    }
}

pub fn send_raw(state: &Arc<Mutex<MidiState>>, bytes: &[u8]) -> Result<(), String> {
    let mut locked = state.lock().map_err(|err| err.to_string())?;
    match locked.output.as_mut() {
        Some(conn) => conn.send(bytes).map_err(|err| err.to_string()),
        None => Err("not connected".to_string()),
    }
}
