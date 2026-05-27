# Editor UX Overhaul — Design Spec

**Date:** 2026-05-27  
**Scope:** Tauri desktop editor (`editor/`)  
**Goal:** Make it obvious which preset is loaded, show when it has unsaved edits, allow renaming, and sync the editor knobs/modes when a preset is selected.

---

## Problem Statement

The current editor has three UX problems:

1. **No active preset indicator.** Clicking a preset in the browser loads it on the device but the editor knobs/modes don't update — the user cannot tell what they're editing.
2. **No dirty state.** Moving a knob after loading a preset gives no indication that the preset now has unsaved changes.
3. **Preset browser is confusing.** No highlight on the active slot, no way to rename, Sync/Export/Import buttons are loose with no clear ownership.

---

## Editing Model

**Hybrid:** knobs always send MIDI CC live (user hears changes immediately), but the editor also tracks which preset slot is loaded and whether current state differs from it. An explicit Save button writes back to the slot.

---

## Layout

Single-column, three zones:

```
┌─────────────────────────────────────────────────────────────────┐
│ ⬤ Connected │ [Chorus Lead_]  B2·05  ●         [Save]  [⋯]   │  ← Header bar
├─────────────────────────────────────────────────────────────────┤
│  [MOD · Chorus]    [DELAY · Tape]    [REVERB · Hall]           │  ← Stage cards
├─────────────────────────────────────────────────────────────────┤
│  ▾ Presets — Bank 2                                            │  ← Collapsible browser
│  00 init  01 lead  [05 Chorus Lead]  06 –  …                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Header Bar (`PresetHeader.tsx` — new component)

**Left side:** connection dot (cyan = connected, grey = disconnected) + port selector (collapses to dot-only when connected).

**Centre:** 
- Preset name — click to edit inline (`<input>`, confirm on Enter or blur). Disabled when no preset is loaded.
- `B{bank}·{slot}` badge — shows active slot address.
- Amber `●` dirty indicator — visible only when `isDirty === true`.

**Right side:**
- `Save` button — primary style, disabled when `!isDirty || !connected`.
- `⋯` button — opens dropdown with: Sync All, Export…, Import….

---

## State (`App.tsx`)

New state fields:

| Field | Type | Description |
|---|---|---|
| `activePreset` | `{ bank: number; slot: number } \| null` | Currently loaded slot address |
| `loadedSnapshot` | `LoadedSnapshot \| null` | Param/mode/name values as of last load or save |
| `presetName` | `string` | Current (possibly renamed) name |

```ts
interface LoadedSnapshot {
  modMode: number; delayMode: number; reverbMode: number;
  modParams: number[]; delayParams: number[]; reverbParams: number[];
  name: string;
}
```

**`isDirty`** — derived boolean, true when `activePreset !== null` and any of the following differ from `loadedSnapshot` (norm values compared rounded to 2 decimal places):
- mode indices
- any of the 21 knob values
- `presetName`

---

## Preset Codec (`src/lib/presetCodec.ts` — new file)

Parses/builds the 92-byte `MultiPresetSlot` struct (little-endian):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `valid` |
| 1 | 1 | `mod_mode` |
| 2 | 1 | `delay_mode` |
| 3 | 1 | `reverb_mode` |
| 4–31 | 28 | `mod_norm[7]` — float32-LE |
| 32–59 | 28 | `delay_norm[7]` — float32-LE |
| 60–87 | 28 | `reverb_norm[7]` — float32-LE |
| 88–90 | 3 | `fx_enabled[3]` |
| 91 | 1 | padding |

Exports:

```ts
function parsePresetSlot(raw: Uint8Array): ParsedPreset
function buildRawData(preset: ParsedPreset): Uint8Array
```

Where `ParsedPreset` is:

```ts
interface ParsedPreset {
  modMode: number; delayMode: number; reverbMode: number;
  modParams: number[]; delayParams: number[]; reverbParams: number[];
}
```

---

## `useMidi.ts` Changes

**New action: `loadPreset(bank, slot)`**

1. Send `SET_ACTIVE` to device immediately.
2. If `presets[bank * 10 + slot]` is already in cache: parse and return `ParsedPreset` synchronously.
3. If not in cache: send `GET_PRESET` — caller awaits the `PRESET_DATA` SysEx response (resolved via a Promise keyed on `bank * 10 + slot`).

**ACK parsing (`midi-sysex` handler):**  
Parse cmd `0x83` (ACK from device). Resolve the pending `savePreset` Promise with `true` (success) or `false` (failure) so the caller can clear dirty state.

**Renamed/new actions:**
- `loadPreset(bank, slot): Promise<ParsedPreset | null>` — resolves immediately for cached slots, or after `PRESET_DATA` SysEx arrives for uncached ones (2 s timeout → resolves `null`).
- `savePreset(bank, slot, name, rawData): Promise<boolean>` — sends PUT_PRESET, resolves `true` on success ACK, `false` on failure ACK or 2 s timeout.

Existing `putPreset` is replaced by `savePreset`.

---

## Preset Browser (`PresetBrowser.tsx`) Changes

- Accept `activePreset: { bank: number; slot: number } | null` prop.
- Highlight active slot card with cyan border + faint background.
- Dim unsynced (null) slots with `opacity-50` and an italic `—`.
- Null slots show a subtle spinner when a `GET_PRESET` is in-flight for them.
- Collapse toggle: chevron row at top, `isOpen` state local to component, defaults open.
- Remove the standalone Sync/Export/Import buttons (moved to `⋯` menu in header).

---

## Interaction Flows

### Load a cached preset
1. User clicks slot card → `loadPreset(bank, slot)` called.
2. `SET_ACTIVE` sent to device.
3. `parsePresetSlot(rawData)` → update `modMode`, `delayMode`, `reverbMode`, `modParams`, `delayParams`, `reverbParams`, `presetName`.
4. Set `activePreset = { bank, slot }` and `loadedSnapshot` to match.
5. `isDirty` immediately becomes `false`.

### Load an unsynced preset
1. User clicks null slot → spinner shown on card.
2. `GET_PRESET` + `SET_ACTIVE` sent in parallel.
3. When `PRESET_DATA` SysEx arrives → cache updated → same steps 3–5 above.

### Edit (knob or mode change)
- CC or SET_MODE sent immediately (existing behaviour).
- `isDirty` recalculated — amber `●` appears.

### Rename
- Click name in header → `<input>` shown with current name.
- Enter or blur → update `presetName` → `isDirty` recalculated.

### Save
1. `Save` clicked → `savePreset(bank, slot, presetName, buildRawData(...))` invoked.
2. Button shows loading state.
3. On ACK `0x83` success → update `loadedSnapshot` to current state → `isDirty` clears → button re-enables.
4. On error → show error toast, button re-enables.

---

## Error Handling

- `GET_PRESET` timeout (no response in 2 s): show error state on slot card, allow retry via click.
- `PUT_PRESET` ACK failure (`ok = 0x01`): toast "Save failed — device returned error". Do not clear dirty.
- Disconnect while dirty: show `●` but disable Save. On reconnect Save re-enables.

---

## Files Changed

| File | Change |
|---|---|
| `src/lib/presetCodec.ts` | New — struct parser/builder |
| `src/components/PresetHeader.tsx` | New — header bar component |
| `src/hooks/useMidi.ts` | Add ACK parsing, `loadPreset` action |
| `src/App.tsx` | Add `activePreset`, `loadedSnapshot`, `presetName`, `isDirty`; wire `loadPreset` and Save |
| `src/components/PresetBrowser.tsx` | Active highlight, collapse toggle, dimmed null slots, remove old action buttons |
