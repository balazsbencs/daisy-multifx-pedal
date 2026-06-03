# Preset System Renovation — Design Spec

**Date:** 2026-06-03  
**Scope:** Firmware (C++), Tauri backend (Rust), Editor frontend (TypeScript/React)  
**Goal:** Fix all broken preset save/load flows and add real-time bidirectional sync between hardware and editor.

---

## 1. Problem Summary

Six concrete bugs make the editor non-functional for preset management:

| # | Layer | Bug |
|---|-------|-----|
| 1 | Firmware | `GET_ALL` (cmd `0x05`) blasts 100 × ~124 bytes at once, overflowing the USB TX FIFO (~512 bytes). "Sync All" silently drops all presets. |
| 2 | Firmware | Browse-mode hardware save never passes a name to `SaveSlot`. Newly saved slots get a blank name. |
| 3 | Firmware | No SysEx command to query current live state. Editor cannot know what the hardware is doing after connect. |
| 4 | Editor | No auto-sync on connect. `presets` starts as 100 × `null`, browser is blank, `activePreset` is never set, Save button stays disabled forever. |
| 5 | Editor | `savePreset` never updates `presetsRef` / `presets` state after a successful save. Browser shows stale name until next sync. |
| 6 | Editor | `handleImportDone` sends data to firmware but never updates local `presets` state. Browser stays blank after import. |

Additionally, the editor has no real-time visibility into hardware control changes (encoder turns, mode switches, footswitch toggles).

---

## 2. Architecture

Three-layer system unchanged:

```
Hardware (C++) ──MIDI SysEx/CC──▶ Tauri backend (Rust) ──Tauri events──▶ Editor (TypeScript/React)
     ◀──────────────────────────────────────────────────────────────────────────────────
```

Changes touch all three layers. No new dependencies introduced.

---

## 3. New SysEx Protocol

Two new messages added to the existing `0x7D` manufacturer namespace.

### 3.1 `GET_LIVE_STATE` (cmd `0x0B`) — Editor → Firmware

```
F0 7D 0B F7
```

Requests the firmware's current live state. Firmware responds with a `LIVE_STATE` frame.

### 3.2 `LIVE_STATE` (cmd `0x82`) — Firmware → Editor

```
F0 7D 82 <active_bank:1> <active_slot:1> <encoded_slot:106> F7
```

- `active_bank`, `active_slot`: which preset slot is currently active (0-indexed)
- `encoded_slot`: 7-bit encoded `MultiPresetSlot` (92 bytes → 106 bytes), same codec as `PRESET_DATA` (`0x81`)
- Total frame: 111 bytes

Sent in two situations:
1. **Response** to `GET_LIVE_STATE` from the editor
2. **Spontaneous broadcast** when hardware controls change (encoder, footswitch, mode switch, preset load from browse) — debounced at 100 ms

**Feedback loop prevention:** The firmware does NOT broadcast `LIVE_STATE` when state changes arrive via MIDI CC or SysEx from the editor. Only hardware-initiated changes trigger a broadcast.

### 3.3 Existing commands unchanged

All existing commands (`0x01`–`0x08`, `0x81`, `0x83`) remain byte-for-byte compatible.

---

## 4. Firmware Changes (`src/`)

### 4.1 `midi_handler.h` / `midi_handler.cpp`

Add to `MultiMidiState`:
```cpp
bool request_live_state = false;
```

Add to `MidiHandlerPedal`:
```cpp
void SendLiveState(int bank, int slot, const MultiPresetSlot& state);
```

In `HandleSysEx`, handle cmd `0x0B`:
```cpp
case 0x0Bu:
    out.request_live_state = true;
    SendAck(cmd, true);
    break;
```

`SendLiveState` builds and sends the `0x82` frame:
```
F0 7D 82 bank slot encoded[106] F7
```
Uses existing `Encode7bit` and the passed `MultiPresetSlot`.

### 4.2 `main.cpp`

**New dirty flag for hardware → editor broadcast** (separate from `live_state_dirty` which drives QSPI auto-save):

```cpp
static bool     hw_live_state_dirty        = false;
static uint32_t hw_last_hw_change_ms       = 0;
constexpr uint32_t kLiveBroadcastDebounceMs = 100u;
```

Set `hw_live_state_dirty = true; hw_last_hw_change_ms = now;` when hardware controls cause state changes:
- Parameter encoder turns
- Mode encoder rotation (mode switches)
- FX footswitch toggles (in Normal mode)
- Preset loads initiated by browse-mode navigation

Do **not** set it for MIDI CC / SysEx changes from the editor.

After the MIDI poll, handle new flags:
```cpp
if (midi.request_live_state) {
    midi_handler.SendLiveState(preset_bank, preset_slot, SnapshotLiveState());
}
```

Broadcast loop (after existing `live_state_dirty` auto-save block):
```cpp
if (hw_live_state_dirty && (now - hw_last_hw_change_ms) >= kLiveBroadcastDebounceMs) {
    midi_handler.SendLiveState(preset_bank, preset_slot, SnapshotLiveState());
    hw_live_state_dirty = false;
}
```

**Browse-mode save fixes:**

When `ctrl.fx_pressed[1]` in browse mode:
- Pass existing name (or `"Preset"` if blank) to `SaveSlot`
- Set `browse_saved_ms = now` for display feedback
- Set `hw_live_state_dirty = true` so editor sees the newly confirmed active state

**Display save feedback:**

Pass a `PresetUiEvent::Saved` event to `display.Update()` for 1 s after browse-mode save. Display shows "SAVED" briefly.

---

## 5. Tauri Backend Changes (`editor/src-tauri/src/`)

### 5.1 `sysex.rs`

Add:
```rust
pub fn build_get_live_state() -> Vec<u8> {
    vec![0xF0, 0x7D, 0x0B, 0xF7]
}
```

### 5.2 `commands.rs`

Replace `get_all_presets` with `sync_all_presets`:
```rust
#[tauri::command]
pub fn sync_all_presets(state: State<SharedMidi>, app: AppHandle) -> Result<(), String> {
    let state = Arc::clone(&state);
    std::thread::spawn(move || {
        for bank in 0u8..10 {
            for slot in 0u8..10 {
                let idx = bank * 10 + slot + 1;
                if midi::send_raw(&state, &sysex::build_get_preset(bank, slot)).is_err() { break; }
                let _ = app.emit("midi-sync-progress", idx as u32);
                std::thread::sleep(std::time::Duration::from_millis(25));
            }
        }
        let _ = app.emit("midi-sync-done", ());
    });
    Ok(())
}
```

Add `get_live_state` command:
```rust
#[tauri::command]
pub fn get_live_state(state: State<SharedMidi>) -> Result<(), String> {
    midi::send_raw(&state, &sysex::build_get_live_state())
}
```

### 5.3 `midi.rs` — auto-query on connect

After establishing the output connection in `connect()`, immediately send `GET_LIVE_STATE`. The existing lock block already releases before this call:
```rust
{
    let mut s = state.lock().unwrap();
    s.output = Some(conn);
    s.input_active = true;
} // lock released here
// New: prime the editor with current hardware state
let _ = midi::send_raw(&state, &sysex::build_get_live_state());
// existing: spawn input thread...
```

The firmware responds with `0x82 LIVE_STATE`, which the existing `handle_incoming` already forwards to the frontend via `midi-sysex`.

### 5.4 `lib.rs`

Register `sync_all_presets`, `get_live_state` in the Tauri builder's `invoke_handler`.
Remove `get_all_presets` from registration.

---

## 6. Editor Changes (`editor/src/`)

### 6.1 `hooks/useMidi.ts`

**Handle `0x82 LIVE_STATE`** in the `midi-sysex` listener:
```ts
if (cmd === 0x82) {
  if (msg.length < 7) return;
  const bank    = msg[3];
  const slot    = msg[4];
  const rawData = decode7bit(msg.slice(5, msg.length - 1));
  const parsed  = parsePresetSlot(rawData);
  setLiveState({ bank, slot, ...parsed });
}
```

Expose new state atom from `useMidi`:
```ts
const [liveState, setLiveState] = useState<LiveStateUpdate | null>(null);
// returned in the hook's return object
```

**Fix `savePreset` — write through to cache after ACK:**

Extend `SavePending` to carry the preset data so the ACK handler has it:
```ts
interface SavePending {
  resolve: (ok: boolean) => void;
  timer:   ReturnType<typeof setTimeout>;
  bank:    number;
  slot:    number;
  name:    string;
  rawData: Uint8Array;
}
```

In the `0x83` ACK branch of the sysex listener, when `ok`:
```ts
const { resolve, bank, slot, name, rawData } = savePending.current;
savePending.current = null;
if (ok) {
  const updated: PresetData = { bank, slot, name, rawData };
  presetsRef.current[bank * 10 + slot] = updated;
  setPresets(prev => {
    const next = [...prev];
    next[bank * 10 + slot] = updated;
    return next;
  });
}
resolve(ok);
```

**Fix `putPreset` — expose `setPresets` for import flow:**

Add `setPresets` to the hook's return value so `App.tsx` can batch-update state after import without re-fetching.

**Sync progress:**
```ts
const [syncProgress, setSyncProgress] = useState<number | null>(null);

useEffect(() => {
  const u1 = listen<number>("midi-sync-progress", e => setSyncProgress(e.payload));
  const u2 = listen<void>("midi-sync-done",     () => setSyncProgress(null));
  return () => { u1.then(f => f()); u2.then(f => f()); };
}, []);
```

Replace `getAllPresets`:
```ts
const syncAllPresets = useCallback(() => {
  invoke("sync_all_presets").catch(e => reportError(`Sync failed: ${e}`));
}, [reportError]);
```

### 6.2 `App.tsx`

**Apply `liveState` from hardware without echo:**

```ts
useEffect(() => {
  if (!midi.liveState) return;
  const ls = midi.liveState;
  setModMode(ls.modMode);
  setDelayMode(ls.delayMode);
  setReverbMode(ls.reverbMode);
  setModParams([...ls.modParams]);
  setDelayParams([...ls.delayParams]);
  setReverbParams([...ls.reverbParams]);
  setFxEnabled(ls.fxEnabled.map(v => v !== 0) as [boolean, boolean, boolean]);
  setActivePreset({ bank: ls.bank, slot: ls.slot });
  setLoadedSnapshot({ ...ls, name: midi.presets[ls.bank * 10 + ls.slot]?.name ?? "" });
  setSaveError(null);
}, [midi.liveState]);
```

Note: no `midi.sendCC` calls here — hardware already has this state.

**Fix `handleImportDone` — update local state:**
```ts
const handleImportDone = useCallback((imported: (PresetData | null)[]) => {
  const next = [...midi.presets];
  imported.forEach(p => {
    if (!p) return;
    next[p.bank * 10 + p.slot] = p;
    midi.putPreset(p.bank, p.slot, p.name, p.rawData);
  });
  midi.setPresets(next);
}, [midi]);
```

**Allow saving to an empty slot:**

Remove the `loadedSnapshot !== null` guard from `handleSave`. Change `isDirty` to treat a `null` snapshot as always dirty:
```ts
const isDirty =
  activePreset !== null && (
    loadedSnapshot === null ||
    presetName !== loadedSnapshot.name ||
    modMode    !== loadedSnapshot.modMode    ||
    /* ... rest unchanged ... */
  );
```

When the user clicks an empty slot in the browser, keep the current editor state as the working content (don't reset knobs to zero). The firmware sends back a zeroed `MultiPresetSlot` for empty slots (`rawData[0] === 0` is the `valid` byte). Check this in `handlePresetSelect` before applying the result:
```ts
// in handlePresetSelect, after receiving result from loadPreset:
const isEmptySlot = result === null || result.rawData?.[0] === 0;
// rawData is available via the cached PresetData; for uncached slots,
// expose valid byte by checking presetsRef after the response arrives,
// or by adding `valid: raw[0]` to ParsedPreset in presetCodec.ts.
```

Simplest approach: add `valid: number` to `ParsedPreset` in `presetCodec.ts` (`raw[0]`), thread it through `LoadedPresetResult`, and check in `handlePresetSelect`:
```ts
if (result.valid === 0) {
  // Empty slot: keep current knob state, just anchor the slot
  setActivePreset({ bank, slot });
  setLoadedSnapshot(null);
  setPresetName("");
} else {
  // Populated slot: apply loaded state as normal
  setModMode(result.modMode); ...
  setLoadedSnapshot({ ...result });
}
```

**Trigger sequential sync after connect:**

In `handleConnect` in `App.tsx`, after `await midi.connect(portName)` succeeds, call `midi.syncAllPresets()`. The LIVE_STATE arrives first (auto-sent by Tauri on connect) to show the active preset; the full browser sync then fills in over ~2.5 s.

### 6.3 `components/PresetHeader.tsx`

Add a thin sync progress bar below the toolbar:
```tsx
{syncProgress !== null && (
  <div className="h-0.5 bg-zinc-800 rounded-full mt-1">
    <div
      className="h-full bg-cyan-500 rounded-full transition-all"
      style={{ width: `${syncProgress}%` }}
    />
  </div>
)}
```

Update "Sync All" menu item to call `onSyncAll` (now wired to `syncAllPresets`).

---

## 7. Data Flow Summary

### Connect flow
```
Editor connects to port
  → Tauri sends GET_LIVE_STATE automatically
  → Firmware responds with 0x82 LIVE_STATE
  → Editor applies live state: active slot highlighted, knobs set, Save enabled
  → Editor triggers sync_all_presets (sequential, paced)
  → Progress bar fills over ~2.5 s
  → Browser fully populated
```

### Hardware knob turn
```
User turns encoder on hardware
  → hw_live_state_dirty set, 100 ms debounce
  → Firmware sends 0x82 LIVE_STATE
  → Editor updates knobs/modes without sending CC back
```

### Editor knob move
```
User moves knob in editor
  → CC sent to hardware immediately (existing path)
  → Hardware applies CC, does NOT broadcast (midi_change suppresses)
  → isDirty becomes true, Save enabled
```

### Save
```
User hits Save in editor
  → put_preset SysEx sent
  → Firmware saves to QSPI, sends ACK (0x83)
  → Editor: presetsRef updated, presets state updated, loadedSnapshot updated, isDirty → false
```

### Hardware browse-mode save
```
Hold TAP → enter browse, navigate, press DELAY footswitch
  → SaveSlot called with existing name (or "Preset")
  → Display shows "SAVED" for 1 s
  → hw_live_state_dirty set → LIVE_STATE broadcast
  → Editor updates activePreset to confirmed slot
```

---

## 8. What Is Not Changed

- `MultiPresetSlot` binary layout (no QSPI migration needed)
- Existing SysEx commands `0x01`–`0x08`, `0x81`, `0x83`
- CC parameter control (CC 14–34)
- Tap tempo, MIDI clock, hold
- Audio engine, DSP modes
- Display layout (only adds a 1 s "SAVED" event)
