# Desktop Editor App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Tauri 2.x desktop app (`editor/` directory) that lets users edit all 21 parameters in real-time via MIDI CC, browse and manage 100 presets via SysEx, and release cross-platform binaries via GitHub Actions.

**Architecture:** Rust backend handles all MIDI I/O (midir crate) and SysEx framing; it exposes Tauri commands to the React frontend and emits Tauri events when the device sends data. The frontend is reactive — it never polls; it listens to events. CC messages are throttled at 10 ms per knob drag to avoid MIDI flooding.

**Tech Stack:** Tauri 2.x · Rust · midir 0.9 · React 18 · TypeScript · Vite · Tailwind CSS · shadcn/ui · react-knob-headless

**Prerequisites:** Firmware plan must be complete (SysEx protocol must be live on the device) before end-to-end testing. Frontend development can proceed in parallel using a mock MIDI backend.

---

## File Map

```
editor/
├── src-tauri/
│   ├── Cargo.toml
│   ├── tauri.conf.json
│   └── src/
│       ├── main.rs          — Tauri app builder, shared AppState
│       ├── midi.rs          — midir wrapper: port list, connect, send raw bytes
│       ├── sysex.rs         — 7-bit encode/decode, frame builders, response parser
│       └── commands.rs      — Tauri #[command] functions
├── src/
│   ├── App.tsx              — layout shell, MIDI connection bar
│   ├── components/
│   │   ├── StageCard.tsx    — shadcn Card: one per mod/delay/reverb stage
│   │   ├── KnobPanel.tsx    — row of react-knob-headless knobs
│   │   ├── ModeSelector.tsx — shadcn Select for mode choice within a stage
│   │   ├── PresetBrowser.tsx— shadcn Tabs (banks) + Card grid (slots)
│   │   └── ExportDialog.tsx — shadcn Dialog for .multifx file export/import
│   └── hooks/
│       └── useMidi.ts       — wraps Tauri invoke + event listener
├── package.json
├── tsconfig.json
├── vite.config.ts
└── index.html
.github/workflows/
└── editor-release.yml       — GitHub Actions cross-platform release
```

---

## Task 1: Scaffold Tauri Project

**Files:**
- Create: `editor/` directory with Tauri 2.x scaffold

- [ ] **Step 1: Scaffold the project**

```bash
cd /Users/bbalazs/daisy/multi-fx
npm create tauri-app@latest editor -- --template react-ts --manager npm
cd editor
npm install
```

- [ ] **Step 2: Verify dev mode opens a window**

```bash
cd editor
npm run tauri dev
```
Expected: a Tauri window opens with the default Vite+React template. Close it.

- [ ] **Step 3: Add midir to Cargo.toml**

Open `editor/src-tauri/Cargo.toml` and add to `[dependencies]`:
```toml
midir = "0.9"
```

- [ ] **Step 4: Install frontend dependencies**

```bash
cd editor
npm install react-knob-headless
npx shadcn@latest init
```

When shadcn asks: choose **Default** style, **Zinc** base color, CSS variables **yes**.

- [ ] **Step 5: Add Tailwind**

```bash
cd editor
npm install -D tailwindcss postcss autoprefixer
npx tailwindcss init -p
```

Update `editor/tailwind.config.js` content field:
```js
/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: { extend: {} },
  plugins: [],
}
```

Add to top of `editor/src/index.css`:
```css
@tailwind base;
@tailwind components;
@tailwind utilities;
```

- [ ] **Step 6: Commit scaffold**

```bash
cd /Users/bbalazs/daisy/multi-fx
git add editor/
git commit -m "feat: scaffold Tauri 2.x editor app with React/TS/shadcn/Tailwind"
```

---

## Task 2: Rust MIDI Backend

**Files:**
- Create: `editor/src-tauri/src/midi.rs`
- Modify: `editor/src-tauri/src/main.rs`

- [ ] **Step 1: Create `editor/src-tauri/src/midi.rs`**

```rust
use midir::{MidiInput, MidiOutput, MidiOutputConnection, Ignore};
use std::sync::{Arc, Mutex};
use tauri::AppHandle;

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
    let midi_out = MidiOutput::new("multi-fx-list").unwrap_or_else(|_| return MidiOutput::new("").unwrap());
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

    // Input connection (spawned — lives for the duration of the session)
    let mut midi_in = MidiInput::new("multi-fx-in").map_err(|e| e.to_string())?;
    midi_in.ignore(Ignore::None); // don't ignore SysEx
    let in_ports = midi_in.ports();
    let in_port = in_ports
        .iter()
        .find(|p| midi_in.port_name(p).ok().as_deref() == Some(port_name))
        .ok_or_else(|| "input port not found".to_string())?
        .clone();

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

    Ok(())
}

fn handle_incoming(message: &[u8], app: &AppHandle, _state: &Arc<Mutex<MidiState>>) {
    if message.is_empty() { return; }
    // SysEx response: F0 7D <cmd> ...
    if message[0] == 0xF0 && message.len() >= 3 && message[1] == 0x7D {
        let _ = app.emit("midi-sysex", message.to_vec());
    }
}

pub fn send_raw(state: &Arc<Mutex<MidiState>>, bytes: &[u8]) -> Result<(), String> {
    let mut s = state.lock().unwrap();
    match s.output.as_mut() {
        Some(conn) => conn.send(bytes).map_err(|e| e.to_string()),
        None => Err("not connected".into()),
    }
}
```

- [ ] **Step 2: Update `editor/src-tauri/src/main.rs` to register AppState**

Replace its content with:

```rust
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
        .invoke_handler(tauri::generate_handler![
            commands::list_midi_ports,
            commands::connect_midi,
            commands::send_cc,
            commands::get_preset,
            commands::put_preset,
            commands::get_all_presets,
            commands::set_active_preset,
            commands::set_mode,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

- [ ] **Step 3: Build Rust (backend only) to confirm it compiles**

```bash
cd editor/src-tauri
cargo build 2>&1 | tail -20
```
Expected: compiles without errors (warnings about unused items are fine).

---

## Task 3: SysEx Codec in Rust

**Files:**
- Create: `editor/src-tauri/src/sysex.rs`

- [ ] **Step 1: Create `editor/src-tauri/src/sysex.rs`**

```rust
/// Encode binary bytes into MIDI SysEx-safe 7-bit format.
/// Every 7 input bytes → 8 output bytes (1 MSB byte + 7 data bytes).
pub fn encode_7bit(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(((input.len() + 6) / 7) * 8);
    for chunk in input.chunks(7) {
        let msb: u8 = chunk
            .iter()
            .enumerate()
            .fold(0u8, |acc, (i, &b)| acc | (((b >> 7) & 1) << i));
        out.push(msb);
        for &b in chunk {
            out.push(b & 0x7F);
        }
    }
    out
}

/// Decode 7-bit-encoded SysEx bytes back to binary.
pub fn decode_7bit(input: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < input.len() {
        let remaining = input.len() - i;
        if remaining < 2 { break; }
        let chunk_len = (remaining - 1).min(7);
        let msb = input[i]; i += 1;
        for j in 0..chunk_len {
            out.push(input[i] | (((msb >> j) & 1) << 7));
            i += 1;
        }
    }
    out
}

pub fn build_get_preset(bank: u8, slot: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x01, bank, slot, 0xF7]
}

pub fn build_put_preset(bank: u8, slot: u8, name: &str, raw_data: &[u8; 92]) -> Vec<u8> {
    let mut name_bytes = [0u8; 12];
    for (i, b) in name.bytes().take(11).enumerate() { name_bytes[i] = b; }
    let encoded = encode_7bit(raw_data);
    let mut frame = vec![0xF0, 0x7D, 0x02, bank, slot];
    frame.extend_from_slice(&name_bytes);
    frame.extend_from_slice(&encoded);
    frame.push(0xF7);
    frame
}

pub fn build_set_active(bank: u8, slot: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x04, bank, slot, 0xF7]
}

pub fn build_get_all() -> Vec<u8> {
    vec![0xF0, 0x7D, 0x05, 0xF7]
}

pub fn build_set_mode(stage: u8, mode_index: u8) -> Vec<u8> {
    vec![0xF0, 0x7D, 0x07, stage, mode_index, 0xF7]
}

pub fn build_cc(channel: u8, cc: u8, value: u8) -> Vec<u8> {
    vec![0xB0 | (channel & 0x0F), cc, value & 0x7F]
}

/// Parse a PRESET_DATA response (cmd 0x81).
/// Returns (bank, slot, name, raw_92_bytes) or None if malformed.
pub fn parse_preset_data(frame: &[u8]) -> Option<(u8, u8, String, Vec<u8>)> {
    // frame: F0 7D 81 bank slot name[12] encoded[106] F7
    if frame.len() < 5 || frame[0] != 0xF0 || frame[1] != 0x7D || frame[2] != 0x81 { return None; }
    let bank = frame[3];
    let slot = frame[4];
    if frame.len() < 5 + 12 { return None; }
    let name = String::from_utf8_lossy(&frame[5..17]).trim_end_matches('\0').to_string();
    let raw = decode_7bit(&frame[17..frame.len().saturating_sub(1)]);
    Some((bank, slot, name, raw))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_92_bytes() {
        let input: Vec<u8> = (0u8..=91).collect();
        let encoded = encode_7bit(&input);
        let decoded = decode_7bit(&encoded);
        assert_eq!(decoded, input);
        // 92 bytes: 13 full groups (104 bytes) + 1 remainder (2 bytes) = 106
        let n = input.len();
        let expected = (n / 7) * 8 + if n % 7 > 0 { n % 7 + 1 } else { 0 };
        assert_eq!(encoded.len(), expected);
    }

    #[test]
    fn all_high_bits() {
        let input = vec![0xFF; 92];
        let encoded = encode_7bit(&input);
        assert!(encoded.iter().all(|&b| b < 0x80), "all output bytes must be < 0x80");
        let decoded = decode_7bit(&encoded);
        assert_eq!(decoded, input);
    }
}
```

- [ ] **Step 2: Run tests**

```bash
cd editor/src-tauri
cargo test 2>&1
```
Expected:
```
test sysex::tests::round_trip_92_bytes ... ok
test sysex::tests::all_high_bits ... ok
```

- [ ] **Step 3: Commit**

```bash
cd /Users/bbalazs/daisy/multi-fx
git add editor/src-tauri/src/sysex.rs
git commit -m "feat: Rust SysEx 7-bit codec with round-trip tests"
```

---

## Task 4: Tauri Commands

**Files:**
- Create: `editor/src-tauri/src/commands.rs`

- [ ] **Step 1: Create `editor/src-tauri/src/commands.rs`**

```rust
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
/// cc: 14–34 for params (matching firmware CC_*_BASE constants).
/// value: 0–127.
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

/// name: up to 11 characters. raw_data: exactly 92 bytes (serialized MultiPresetSlot).
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

/// stage: 0=mod 1=delay 2=reverb. mode_index: 0-based index within the stage.
#[tauri::command]
pub fn set_mode(stage: u8, mode_index: u8, state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_set_mode(stage, mode_index))
}
```

- [ ] **Step 2: Build the full Tauri backend**

```bash
cd editor/src-tauri
cargo build 2>&1 | tail -10
```
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
cd /Users/bbalazs/daisy/multi-fx
git add editor/src-tauri/src/
git commit -m "feat: Tauri commands for MIDI CC and SysEx preset operations"
```

---

## Task 5: Install shadcn Components

**Files:**
- Modify: `editor/src/` (shadcn adds component files)

- [ ] **Step 1: Add the shadcn components needed by the app**

```bash
cd editor
npx shadcn@latest add button card select tabs dialog label input
```

- [ ] **Step 2: Confirm components exist**

```bash
ls editor/src/components/ui/
```
Expected: `button.tsx  card.tsx  select.tsx  tabs.tsx  dialog.tsx  label.tsx  input.tsx` (plus index files)

- [ ] **Step 3: Commit**

```bash
cd /Users/bbalazs/daisy/multi-fx
git add editor/src/components/ui/
git commit -m "feat: add shadcn/ui components (button, card, select, tabs, dialog)"
```

---

## Task 6: useMidi Hook

**Files:**
- Create: `editor/src/hooks/useMidi.ts`

- [ ] **Step 1: Create `editor/src/hooks/useMidi.ts`**

```typescript
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState, useCallback } from "react";

// CC base values matching firmware constants.h
const CC_MOD_BASE    = 14;
const CC_DELAY_BASE  = 21;
const CC_REVERB_BASE = 28;

export interface PresetData {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array; // 92 bytes
}

export function useMidi() {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [presets, setPresets] = useState<(PresetData | null)[]>(
    Array(100).fill(null)
  );
  const ccThrottle = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());

  const refreshPorts = useCallback(async () => {
    const p = await invoke<string[]>("list_midi_ports");
    setPorts(p);
  }, []);

  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    setConnected(true);
  }, []);

  // Send CC with 10ms throttle per knob.
  const sendCC = useCallback(
    (stage: "mod" | "delay" | "reverb", paramIndex: number, normalised: number) => {
      const base =
        stage === "mod" ? CC_MOD_BASE :
        stage === "delay" ? CC_DELAY_BASE : CC_REVERB_BASE;
      const cc    = base + paramIndex;
      const value = Math.round(normalised * 127);
      const key   = `${cc}`;

      if (ccThrottle.current.has(key)) clearTimeout(ccThrottle.current.get(key)!);
      ccThrottle.current.set(
        key,
        setTimeout(() => {
          invoke("send_cc", { channel: 0, cc, value }).catch(console.error);
          ccThrottle.current.delete(key);
        }, 10)
      );
    },
    []
  );

  const setActivePreset = useCallback((bank: number, slot: number) => {
    invoke("set_active_preset", { bank, slot }).catch(console.error);
  }, []);

  const setMode = useCallback((stage: number, modeIndex: number) => {
    invoke("set_mode", { stage, modeIndex }).catch(console.error);
  }, []);

  const getAllPresets = useCallback(() => {
    invoke("get_all_presets").catch(console.error);
  }, []);

  const putPreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array) => {
      invoke("put_preset", {
        bank,
        slot,
        name,
        rawData: Array.from(rawData),
      }).catch(console.error);
    },
    []
  );

  // Listen for incoming SysEx from the device.
  useEffect(() => {
    const unlisten = listen<number[]>("midi-sysex", (event) => {
      const msg = new Uint8Array(event.payload);
      // PRESET_DATA response: F0 7D 81 bank slot name[12] encoded[106] F7
      if (msg.length >= 5 && msg[0] === 0xf0 && msg[1] === 0x7d && msg[2] === 0x81) {
        const bank = msg[3];
        const slot = msg[4];
        const nameBytes = msg.slice(5, 17);
        const name = new TextDecoder()
          .decode(nameBytes)
          .replace(/\0+$/, "");
        const encoded = msg.slice(17, msg.length - 1);
        const rawData = decode7bit(encoded);
        const idx = bank * 10 + slot;
        setPresets((prev) => {
          const next = [...prev];
          next[idx] = { bank, slot, name, rawData };
          return next;
        });
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, []);

  return {
    ports,
    connected,
    presets,
    refreshPorts,
    connect,
    sendCC,
    setActivePreset,
    setMode,
    getAllPresets,
    putPreset,
  };
}

// Mirror of the Rust decode_7bit (runs in the browser for incoming SysEx).
function decode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  let i = 0;
  while (i < input.length) {
    const remaining = input.length - i;
    if (remaining < 2) break;
    const chunkLen = Math.min(remaining - 1, 7);
    const msb = input[i++];
    for (let j = 0; j < chunkLen; j++) {
      out.push(input[i++] | (((msb >> j) & 1) << 7));
    }
  }
  return new Uint8Array(out);
}
```

- [ ] **Step 2: Build to check TypeScript types**

```bash
cd editor
npm run build 2>&1 | tail -10
```
Expected: no TypeScript errors in `useMidi.ts` (UI components are not yet imported so there may be other errors — that's fine at this stage).

---

## Task 7: KnobPanel Component

**Files:**
- Create: `editor/src/components/KnobPanel.tsx`

- [ ] **Step 1: Create `editor/src/components/KnobPanel.tsx`**

```tsx
import { useCallback } from "react";
import { KnobHeadless, KnobHeadlessLabel, KnobHeadlessOutput } from "react-knob-headless";

const PARAM_NAMES = ["Speed/Time", "Depth/Repeats", "Mix", "Tone/Filter", "P1/Grit", "P2/ModSpd", "Level/ModDep"];

interface KnobPanelProps {
  stage: "mod" | "delay" | "reverb";
  values: number[]; // 7 normalised [0,1] values
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
}

export function KnobPanel({ stage, values, onParamChange }: KnobPanelProps) {
  const handleChange = useCallback(
    (index: number, value: number) => {
      onParamChange(stage, index, value);
    },
    [stage, onParamChange]
  );

  return (
    <div className="grid grid-cols-4 gap-3 p-2">
      {values.slice(0, 7).map((value, i) => (
        <div key={i} className="flex flex-col items-center gap-1">
          <KnobHeadless
            value={value}
            min={0}
            max={1}
            step={0.01}
            aria-label={PARAM_NAMES[i]}
            className="w-12 h-12 rounded-full bg-zinc-800 border border-zinc-600 cursor-pointer"
            onValueChange={(v) => handleChange(i, v)}
          >
            {({ percentage }) => (
              <svg viewBox="0 0 40 40" className="w-full h-full">
                <circle cx="20" cy="20" r="16" fill="none" stroke="#3f3f46" strokeWidth="4" />
                <circle
                  cx="20" cy="20" r="16"
                  fill="none"
                  stroke="#a1a1aa"
                  strokeWidth="4"
                  strokeDasharray={`${percentage * 75.4} 100`}
                  strokeLinecap="round"
                  transform="rotate(-220 20 20)"
                />
                <circle cx="20" cy="20" r="4" fill="#e4e4e7" />
              </svg>
            )}
          </KnobHeadless>
          <KnobHeadlessOutput className="text-xs text-zinc-400 text-center truncate w-14">
            {(value * 100).toFixed(0)}
          </KnobHeadlessOutput>
          <span className="text-[10px] text-zinc-500 text-center truncate w-14">
            {PARAM_NAMES[i]}
          </span>
        </div>
      ))}
    </div>
  );
}
```

- [ ] **Step 2: Confirm no TypeScript errors**

```bash
cd editor
npx tsc --noEmit 2>&1 | grep "KnobPanel" | head -5
```
Expected: no errors mentioning `KnobPanel.tsx`.

---

## Task 8: StageCard and ModeSelector

**Files:**
- Create: `editor/src/components/ModeSelector.tsx`
- Create: `editor/src/components/StageCard.tsx`

- [ ] **Step 1: Create `editor/src/components/ModeSelector.tsx`**

```tsx
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";

const MOD_MODES    = ["Chorus","Flanger","Phaser","Tremolo","Vibrato","Auto-Wah"];
const DELAY_MODES  = ["Tape","Digital","Ping-Pong","Reverse","Slapback","Dotted-8th","LoFi","Shimmer","Ducking","Multi"];
const REVERB_MODES = ["Hall","Room","Plate","Spring","Cloud","Shimmer","Magneto","Gated","Freeze","Air","Chapel","Cave"];

const MODE_LISTS: Record<string, string[]> = {
  mod: MOD_MODES,
  delay: DELAY_MODES,
  reverb: REVERB_MODES,
};

interface ModeSelectorProps {
  stage: "mod" | "delay" | "reverb";
  modeIndex: number;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
}

export function ModeSelector({ stage, modeIndex, onModeChange }: ModeSelectorProps) {
  const modes = MODE_LISTS[stage];
  return (
    <Select
      value={String(modeIndex)}
      onValueChange={(v) => onModeChange(stage, Number(v))}
    >
      <SelectTrigger className="w-full">
        <SelectValue placeholder="Select mode" />
      </SelectTrigger>
      <SelectContent>
        {modes.map((name, i) => (
          <SelectItem key={i} value={String(i)}>
            {name}
          </SelectItem>
        ))}
      </SelectContent>
    </Select>
  );
}
```

- [ ] **Step 2: Create `editor/src/components/StageCard.tsx`**

```tsx
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { KnobPanel } from "./KnobPanel";
import { ModeSelector } from "./ModeSelector";

interface StageCardProps {
  title: string;
  stage: "mod" | "delay" | "reverb";
  stageIndex: number;          // 0=mod 1=delay 2=reverb (for SET_MODE)
  modeIndex: number;
  params: number[];            // 7 normalised values
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
}

export function StageCard({
  title,
  stage,
  modeIndex,
  params,
  onParamChange,
  onModeChange,
}: StageCardProps) {
  return (
    <Card className="flex-1 min-w-0">
      <CardHeader className="pb-2">
        <CardTitle className="text-sm font-semibold uppercase tracking-wide text-zinc-400">
          {title}
        </CardTitle>
        <ModeSelector stage={stage} modeIndex={modeIndex} onModeChange={onModeChange} />
      </CardHeader>
      <CardContent className="pt-0">
        <KnobPanel stage={stage} values={params} onParamChange={onParamChange} />
      </CardContent>
    </Card>
  );
}
```

---

## Task 9: PresetBrowser Component

**Files:**
- Create: `editor/src/components/PresetBrowser.tsx`

- [ ] **Step 1: Create `editor/src/components/PresetBrowser.tsx`**

```tsx
import { Tabs, TabsList, TabsTrigger, TabsContent } from "@/components/ui/tabs";
import { Card } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { PresetData } from "../hooks/useMidi";

interface PresetBrowserProps {
  presets: (PresetData | null)[];
  onSelect: (bank: number, slot: number) => void;
  onSyncAll: () => void;
  onExport: () => void;
  onImport: () => void;
}

export function PresetBrowser({
  presets,
  onSelect,
  onSyncAll,
  onExport,
  onImport,
}: PresetBrowserProps) {
  return (
    <div className="border-t border-zinc-800 pt-3">
      <div className="flex items-center justify-between mb-2">
        <span className="text-sm font-semibold text-zinc-400 uppercase tracking-wide">
          Presets
        </span>
        <div className="flex gap-2">
          <Button size="sm" variant="outline" onClick={onSyncAll}>
            Sync All
          </Button>
          <Button size="sm" variant="outline" onClick={onExport}>
            Export
          </Button>
          <Button size="sm" variant="outline" onClick={onImport}>
            Import
          </Button>
        </div>
      </div>

      <Tabs defaultValue="0">
        <TabsList className="mb-2">
          {Array.from({ length: 10 }, (_, b) => (
            <TabsTrigger key={b} value={String(b)}>
              Bank {b}
            </TabsTrigger>
          ))}
        </TabsList>

        {Array.from({ length: 10 }, (_, bank) => (
          <TabsContent key={bank} value={String(bank)}>
            <div className="grid grid-cols-5 gap-2">
              {Array.from({ length: 10 }, (_, slot) => {
                const idx  = bank * 10 + slot;
                const data = presets[idx];
                return (
                  <Card
                    key={slot}
                    className="p-2 cursor-pointer hover:bg-zinc-800 transition-colors"
                    onClick={() => onSelect(bank, slot)}
                  >
                    <div className="text-[10px] text-zinc-500">
                      B{bank}·{String(slot).padStart(2, "0")}
                    </div>
                    <div className="text-xs text-zinc-200 truncate mt-0.5">
                      {data?.name || "—"}
                    </div>
                  </Card>
                );
              })}
            </div>
          </TabsContent>
        ))}
      </Tabs>
    </div>
  );
}
```

---

## Task 10: ExportDialog Component

**Files:**
- Create: `editor/src/components/ExportDialog.tsx`

- [ ] **Step 1: Create `editor/src/components/ExportDialog.tsx`**

```tsx
import { useState } from "react";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogFooter,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { PresetData } from "../hooks/useMidi";

interface ExportDialogProps {
  open: boolean;
  onClose: () => void;
  presets: (PresetData | null)[];
  onImportDone: (presets: (PresetData | null)[]) => void;
}

export function ExportDialog({ open, onClose, presets, onImportDone }: ExportDialogProps) {
  const [importError, setImportError] = useState("");

  const handleExport = () => {
    const banks = Array.from({ length: 10 }, (_, b) => ({
      bank: b,
      slots: Array.from({ length: 10 }, (_, s) => {
        const p = presets[b * 10 + s];
        if (!p) return null;
        return {
          slot: s,
          name: p.name,
          rawData: Array.from(p.rawData),
        };
      }).filter(Boolean),
    }));

    const blob = new Blob(
      [JSON.stringify({ version: 1, device: "multi-fx", banks }, null, 2)],
      { type: "application/json" }
    );
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement("a");
    a.href     = url;
    a.download = "presets.multifx";
    a.click();
    URL.revokeObjectURL(url);
    onClose();
  };

  const handleImport = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const json = JSON.parse(evt.target?.result as string);
        if (json.version !== 1 || json.device !== "multi-fx") {
          setImportError("Not a valid .multifx file");
          return;
        }
        const next: (PresetData | null)[] = Array(100).fill(null);
        for (const bankObj of json.banks) {
          for (const slot of bankObj.slots) {
            if (!slot) continue;
            const idx = bankObj.bank * 10 + slot.slot;
            next[idx] = {
              bank: bankObj.bank,
              slot: slot.slot,
              name: slot.name,
              rawData: new Uint8Array(slot.rawData),
            };
          }
        }
        onImportDone(next);
        setImportError("");
        onClose();
      } catch {
        setImportError("Failed to parse file");
      }
    };
    reader.readAsText(file);
  };

  return (
    <Dialog open={open} onOpenChange={(v) => !v && onClose()}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Export / Import Presets</DialogTitle>
        </DialogHeader>

        <div className="space-y-4 py-2">
          <div>
            <Label>Export all presets to .multifx file</Label>
            <Button className="mt-2 w-full" onClick={handleExport}>
              Download presets.multifx
            </Button>
          </div>

          <div>
            <Label>Import from .multifx file</Label>
            <Input
              className="mt-2"
              type="file"
              accept=".multifx,application/json"
              onChange={handleImport}
            />
            {importError && (
              <p className="text-xs text-red-500 mt-1">{importError}</p>
            )}
          </div>
        </div>

        <DialogFooter>
          <Button variant="outline" onClick={onClose}>
            Close
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
```

---

## Task 11: Wire App.tsx

**Files:**
- Modify: `editor/src/App.tsx`

- [ ] **Step 1: Replace `editor/src/App.tsx` with**

```tsx
import { useState, useCallback } from "react";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Button } from "@/components/ui/button";
import { StageCard } from "./components/StageCard";
import { PresetBrowser } from "./components/PresetBrowser";
import { ExportDialog } from "./components/ExportDialog";
import { useMidi, PresetData } from "./hooks/useMidi";
import { useEffect } from "react";

const DEFAULT_PARAMS = Array(7).fill(0.5) as number[];

export default function App() {
  const midi = useMidi();
  const [selectedPort, setSelectedPort] = useState("");
  const [modMode,    setModMode]    = useState(0);
  const [delayMode,  setDelayMode]  = useState(0);
  const [reverbMode, setReverbMode] = useState(0);
  const [modParams,    setModParams]    = useState<number[]>([...DEFAULT_PARAMS]);
  const [delayParams,  setDelayParams]  = useState<number[]>([...DEFAULT_PARAMS]);
  const [reverbParams, setReverbParams] = useState<number[]>([...DEFAULT_PARAMS]);
  const [exportOpen, setExportOpen] = useState(false);

  useEffect(() => { midi.refreshPorts(); }, []);

  const handleConnect = async () => {
    if (!selectedPort) return;
    await midi.connect(selectedPort);
  };

  const handleParamChange = useCallback(
    (stage: "mod" | "delay" | "reverb", index: number, value: number) => {
      if (stage === "mod")    setModParams(p    => { const n = [...p]; n[index] = value; return n; });
      if (stage === "delay")  setDelayParams(p  => { const n = [...p]; n[index] = value; return n; });
      if (stage === "reverb") setReverbParams(p => { const n = [...p]; n[index] = value; return n; });
      midi.sendCC(stage, index, value);
    },
    [midi]
  );

  const handleModeChange = useCallback(
    (stage: "mod" | "delay" | "reverb", index: number) => {
      if (stage === "mod")    setModMode(index);
      if (stage === "delay")  setDelayMode(index);
      if (stage === "reverb") setReverbMode(index);
      const stageIndex = stage === "mod" ? 0 : stage === "delay" ? 1 : 2;
      midi.setMode(stageIndex, index);
    },
    [midi]
  );

  const handleImportDone = useCallback(
    (_imported: (PresetData | null)[]) => {
      // Send all imported presets to device sequentially.
      _imported.forEach((p) => {
        if (!p) return;
        midi.putPreset(p.bank, p.slot, p.name, p.rawData);
      });
    },
    [midi]
  );

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100 p-4 flex flex-col gap-4">
      {/* Connection bar */}
      <div className="flex items-center gap-2">
        <Select value={selectedPort} onValueChange={setSelectedPort}>
          <SelectTrigger className="w-64">
            <SelectValue placeholder="Select MIDI port" />
          </SelectTrigger>
          <SelectContent>
            {midi.ports.map((p) => (
              <SelectItem key={p} value={p}>{p}</SelectItem>
            ))}
          </SelectContent>
        </Select>
        <Button onClick={handleConnect} disabled={!selectedPort || midi.connected}>
          {midi.connected ? "Connected" : "Connect"}
        </Button>
        <Button variant="outline" onClick={midi.refreshPorts}>
          Refresh
        </Button>
      </div>

      {/* Stage cards */}
      <div className="flex gap-4">
        <StageCard title="Mod"    stage="mod"    stageIndex={0} modeIndex={modMode}    params={modParams}    onParamChange={handleParamChange} onModeChange={handleModeChange} />
        <StageCard title="Delay"  stage="delay"  stageIndex={1} modeIndex={delayMode}  params={delayParams}  onParamChange={handleParamChange} onModeChange={handleModeChange} />
        <StageCard title="Reverb" stage="reverb" stageIndex={2} modeIndex={reverbMode} params={reverbParams} onParamChange={handleParamChange} onModeChange={handleModeChange} />
      </div>

      {/* Preset browser */}
      <PresetBrowser
        presets={midi.presets}
        onSelect={(bank, slot) => midi.setActivePreset(bank, slot)}
        onSyncAll={midi.getAllPresets}
        onExport={() => setExportOpen(true)}
        onImport={() => setExportOpen(true)}
      />

      <ExportDialog
        open={exportOpen}
        onClose={() => setExportOpen(false)}
        presets={midi.presets}
        onImportDone={handleImportDone}
      />
    </div>
  );
}
```

- [ ] **Step 2: Run the dev build to check for type errors**

```bash
cd editor
npm run tauri dev 2>&1 | head -30
```
Expected: app window opens, no TypeScript compile errors in the terminal.

- [ ] **Step 3: Commit**

```bash
cd /Users/bbalazs/daisy/multi-fx
git add editor/src/
git commit -m "feat: complete desktop editor UI (knobs, stage cards, preset browser, export dialog)"
```

---

## Task 12: GitHub Actions Release Workflow

**Files:**
- Create: `.github/workflows/editor-release.yml`

- [ ] **Step 1: Ensure `.github/workflows/` exists**

```bash
mkdir -p /Users/bbalazs/daisy/multi-fx/.github/workflows
```

- [ ] **Step 2: Create `.github/workflows/editor-release.yml`**

```yaml
name: Editor Release

on:
  push:
    tags:
      - "editor-v*"

jobs:
  release:
    permissions:
      contents: write
    strategy:
      fail-fast: false
      matrix:
        include:
          - platform: macos-latest
            args: "--target universal-apple-darwin"
          - platform: windows-latest
            args: ""
          - platform: ubuntu-22.04
            args: ""

    runs-on: ${{ matrix.platform }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup Node
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
          cache-dependency-path: editor/package-lock.json

      - name: Install Rust stable
        uses: dtolnay/rust-toolchain@stable
        with:
          targets: ${{ matrix.platform == 'macos-latest' && 'aarch64-apple-darwin,x86_64-apple-darwin' || '' }}

      - name: Cache Rust
        uses: swatinem/rust-cache@v2
        with:
          workspaces: editor/src-tauri -> target

      - name: Install Linux dependencies
        if: matrix.platform == 'ubuntu-22.04'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libwebkit2gtk-4.1-dev \
            libappindicator3-dev \
            librsvg2-dev \
            patchelf \
            libasound2-dev

      - name: Install frontend dependencies
        run: npm ci
        working-directory: editor

      - name: Build and release
        uses: tauri-apps/tauri-action@v0
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        with:
          projectPath: editor
          tagName: ${{ github.ref_name }}
          releaseName: "Multi-FX Editor ${{ github.ref_name }}"
          releaseBody: |
            ## Multi-FX Editor ${{ github.ref_name }}

            Desktop preset editor for the multi-fx Daisy Seed pedal.
            Requires firmware with SysEx support (firmware plan complete).

            Connect via USB MIDI and select the "multi-fx" port.
          releaseDraft: false
          prerelease: false
          args: ${{ matrix.args }}
```

- [ ] **Step 3: Verify the workflow file is valid YAML**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/editor-release.yml'))" && echo "YAML OK"
```
Expected: `YAML OK`

- [ ] **Step 4: Commit and push**

```bash
git add .github/workflows/editor-release.yml
git commit -m "ci: add GitHub Actions editor release pipeline (macOS/Windows/Linux)"
```

- [ ] **Step 5: Test the release pipeline with a tag**

```bash
git tag editor-v0.1.0
git push origin editor-v0.1.0
```

Then open GitHub → Actions → Editor Release to watch the three parallel builds. Expected: all three pass and a GitHub Release is created with `.dmg`, `.msi`, `.AppImage`, and `.deb` assets.
