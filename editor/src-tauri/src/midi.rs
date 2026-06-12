use midir::{MidiInput, MidiOutput, MidiOutputConnection, Ignore};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use tauri::{AppHandle, Emitter};

pub struct MidiState {
    pub output: Option<MidiOutputConnection>,
    pub input_active: bool,
    /// Latest port list, kept up to date by the `watch_ports` thread. Reused by
    /// `list_ports` so that refreshing never creates a transient CoreMIDI client
    /// (doing so repeatedly triggers "MIDI support currently unavailable" on
    /// macOS — see watch_ports).
    pub ports: Vec<String>,
    /// Stop signal for the current input-listener thread. Set to true to make it
    /// exit and drop its MIDI input connection (clean disconnect, no thread leak).
    pub input_stop: Option<Arc<AtomicBool>>,
}

impl MidiState {
    pub fn new() -> Self {
        Self { output: None, input_active: false, ports: Vec::new(), input_stop: None }
    }
}

/// Tear down the active connection: stop the input thread (closing its MIDI
/// input) and drop the output connection. Safe to call when not connected.
pub fn disconnect(state: &Arc<Mutex<MidiState>>, app: &AppHandle) {
    {
        let mut s = state.lock().unwrap();
        if let Some(flag) = s.input_stop.take() { flag.store(true, Ordering::Relaxed); }
        s.output = None;
        s.input_active = false;
    }
    eprintln!("[midi] disconnected");
    let _ = app.emit("midi-input-status", false);
}

/// Return the most recently enumerated port list, cached by `watch_ports`.
/// Never creates a new MIDI client.
pub fn list_ports(state: &Arc<Mutex<MidiState>>) -> Vec<String> {
    state.lock().unwrap().ports.clone()
}

pub fn connect(
    port_name: &str,
    state: Arc<Mutex<MidiState>>,
    app: AppHandle,
) -> Result<(), String> {
    // Output connection
    let midi_out = MidiOutput::new("multi-fx-out").map_err(|e| e.to_string())?;
    let out_names: Vec<String> =
        midi_out.ports().iter().filter_map(|p| midi_out.port_name(p).ok()).collect();
    eprintln!("[midi] connect requested: {port_name:?}");
    eprintln!("[midi] output destinations seen: {out_names:?}");
    let out_port = midi_out
        .ports()
        .into_iter()
        .find(|p| midi_out.port_name(p).ok().as_deref() == Some(port_name))
        .ok_or_else(|| format!("port not found: {port_name}"))?;
    let conn = midi_out.connect(&out_port, "multi-fx-out").map_err(|e| e.to_string())?;
    eprintln!("[midi] output connected to {port_name:?}");

    // Stop signal for this connection's input thread.
    let stop_flag = Arc::new(AtomicBool::new(false));
    {
        let mut s = state.lock().unwrap();
        // Tear down any prior connection first so reconnecting never leaves a
        // stale output handle or a leaked input thread.
        if let Some(flag) = s.input_stop.take() { flag.store(true, Ordering::Relaxed); }
        s.output = Some(conn);
        s.input_active = false; // updated below once we know if input actually connects
        s.input_stop = Some(Arc::clone(&stop_flag));
    }
    // Input connection (optional — no SysEx responses without it, but CC/mode/sysex
    // sending still works with output-only).
    let mut midi_in = match MidiInput::new("multi-fx-in") {
        Ok(mut m) => { m.ignore(Ignore::None); Some(m) }
        Err(_) => None,
    };
    // Try exact name first, then case-insensitive prefix match as fallback.
    // macOS CoreMIDI sometimes presents output/input ports with slightly
    // different names for the same physical device.
    let in_port = midi_in.as_ref().and_then(|m| {
        let ports = m.ports();
        if let Some(p) = ports.iter().find(|p| m.port_name(p).ok().as_deref() == Some(port_name)) {
            return Some(p.clone());
        }
        let prefix = port_name.split_whitespace().next().unwrap_or(port_name).to_lowercase();
        ports.into_iter().find(|p| {
            m.port_name(p).ok()
                .map(|n| n.to_lowercase().starts_with(&prefix))
                .unwrap_or(false)
        })
    });

    eprintln!("[midi] input source matched: {}",
              if in_port.is_some() { "yes" } else { "NO (output-only)" });

    if let (Some(midi_in), Some(in_port)) = (midi_in.take(), in_port) {
        let state_clone    = Arc::clone(&state);
        let state_flag     = Arc::clone(&state);
        let app_for_cb     = app.clone();
        let app_for_status = app.clone();
        let thread_stop    = Arc::clone(&stop_flag);
        std::thread::spawn(move || {
            // midi_in.connect() registers the callback synchronously — once it returns
            // successfully, the input is live and ready to receive messages.
            match midi_in.connect(
                &in_port,
                "multi-fx-in",
                move |_timestamp, message, _| {
                    handle_incoming(message, &app_for_cb, &state_clone);
                },
                (),
            ) {
                Ok(_conn) => {
                    {
                        let mut s = state_flag.lock().unwrap();
                        s.input_active = true;
                    }
                    // Emit AFTER connect() — guarantees input is live before frontend
                    // calls get_live_state.
                    let _ = app_for_status.emit("midi-input-status", true);
                    // Keep _conn alive until disconnect() (or a reconnect) sets the
                    // stop flag; then the connection drops and the input closes.
                    while !thread_stop.load(Ordering::Relaxed) {
                        std::thread::sleep(std::time::Duration::from_millis(200));
                    }
                }
                Err(_) => {
                    let _ = app_for_status.emit("midi-input-status", false);
                }
            }
        });
    } else {
        // No input port found — notify immediately.
        let _ = app.emit("midi-input-status", false);
    }

    Ok(())
}

fn handle_incoming(message: &[u8], app: &AppHandle, _state: &Arc<Mutex<MidiState>>) {
    if message.is_empty() { return; }
    eprintln!("[midi] recv {:02X?} ({} B)", &message[..message.len().min(8)], message.len());
    match message[0] & 0xF0 {
        // Control Change — forward to frontend so hardware knobs update the UI.
        0xB0 if message.len() >= 3 => {
            let _ = app.emit("midi-cc", [message[1], message[2]]);
        }
        // SysEx response: F0 7D <cmd> ...
        0xF0 if message.len() >= 3 && message[1] == 0x7D => {
            let _ = app.emit("midi-sysex", message.to_vec());
        }
        _ => {}
    }
}

/// Spawns a background thread that polls the MIDI output port list every 500 ms.
/// Emits "midi-ports-changed" to the frontend whenever the list changes.
/// This handles devices that enumerate after the app starts, or that briefly
/// drop off and re-appear (e.g. Daisy Seed rebooting after a firmware flash).
///
/// A single MidiOutput client is created once and reused across all ticks.
/// Creating/destroying a CoreMIDI client on every tick caused "MIDI support
/// currently unavailable" errors in concurrent connect_midi calls on macOS.
pub fn watch_ports(app: AppHandle, state: Arc<Mutex<MidiState>>) {
    std::thread::spawn(move || {
        // Retry until CoreMIDI is ready (can be unavailable briefly at app start).
        let midi_out = loop {
            if let Ok(o) = MidiOutput::new("multi-fx-watcher") { break o; }
            std::thread::sleep(std::time::Duration::from_millis(200));
        };
        let mut last: Vec<String> = vec![];
        loop {
            let current: Vec<String> = midi_out
                .ports()
                .iter()
                .filter_map(|p| midi_out.port_name(p).ok())
                .collect();
            if current != last {
                last = current.clone();
                // Cache for list_ports() so refresh never spawns a new client.
                if let Ok(mut s) = state.lock() { s.ports = current.clone(); }
                let _ = app.emit("midi-ports-changed", current);
            }
            std::thread::sleep(std::time::Duration::from_millis(500));
        }
    });
}

pub fn send_raw(state: &Arc<Mutex<MidiState>>, bytes: &[u8]) -> Result<(), String> {
    let mut s = state.lock().unwrap();
    match s.output.as_mut() {
        Some(conn) => {
            let r = conn.send(bytes).map_err(|e| e.to_string());
            eprintln!("[midi] send {:02X?} ({} B) -> {}",
                      &bytes[..bytes.len().min(6)], bytes.len(),
                      match &r { Ok(_) => "ok".to_string(), Err(e) => format!("ERR {e}") });
            r
        }
        None => {
            eprintln!("[midi] send dropped: output not connected");
            Err("not connected".into())
        }
    }
}
