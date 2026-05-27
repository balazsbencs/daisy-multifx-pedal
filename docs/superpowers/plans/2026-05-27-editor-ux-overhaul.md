# Editor UX Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Tauri editor show which preset is loaded, reflect its values in the knobs/modes, indicate unsaved edits, allow renaming, and save back to the slot.

**Architecture:** Add a preset codec utility for parsing/building the 92-byte firmware struct, extend `useMidi` with Promise-based `loadPreset`/`savePreset` actions, add a new `PresetHeader` component that owns connection + active-preset UI, and wire new state (`activePreset`, `loadedSnapshot`, `isDirty`) in `App`.

**Tech Stack:** React 18, TypeScript, Tauri v2, midir (Rust MIDI), shadcn/ui components (`Button`, `Select`), existing `invoke`/`listen` Tauri API.

> **Note — no automated tests:** `CLAUDE.md` documents that this project has no test infrastructure. Verification steps run the app instead.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `editor/src/lib/presetCodec.ts` | Create | Parse/build the 92-byte `MultiPresetSlot` struct |
| `editor/src/hooks/useMidi.ts` | Modify | Add `loadPreset`, `savePreset`, ACK parsing, `presetsRef` |
| `editor/src/components/PresetHeader.tsx` | Create | Connection dot, editable name, badge, dirty dot, Save, ⋯ menu |
| `editor/src/components/PresetBrowser.tsx` | Modify | Active highlight, collapse toggle, dimmed null slots, remove old action buttons |
| `editor/src/App.tsx` | Modify | New state + wiring; remove old connection bar |

---

## Task 1: Preset Codec

**Files:**
- Create: `editor/src/lib/presetCodec.ts`

The 92-byte `MultiPresetSlot` C struct layout (little-endian floats):
- byte 0: `valid` (uint8)
- byte 1: `mod_mode`, byte 2: `delay_mode`, byte 3: `reverb_mode`
- bytes 4–31: `mod_norm[7]` (7 × float32-LE)
- bytes 32–59: `delay_norm[7]`
- bytes 60–87: `reverb_norm[7]`
- bytes 88–90: `fx_enabled[3]`, byte 91: padding

- [ ] **Step 1: Create the file**

```typescript
// editor/src/lib/presetCodec.ts

export interface ParsedPreset {
  modMode: number;
  delayMode: number;
  reverbMode: number;
  modParams: number[];              // 7 normalised [0,1]
  delayParams: number[];            // 7 normalised [0,1]
  reverbParams: number[];           // 7 normalised [0,1]
  fxEnabled: [number, number, number]; // preserved for round-trip
}

function readFloats(view: DataView, offset: number, count: number): number[] {
  return Array.from({ length: count }, (_, i) =>
    Math.max(0, Math.min(1, view.getFloat32(offset + i * 4, true)))
  );
}

export function parsePresetSlot(raw: Uint8Array): ParsedPreset {
  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  return {
    modMode:    raw[1],
    delayMode:  raw[2],
    reverbMode: raw[3],
    modParams:    readFloats(view, 4,  7),
    delayParams:  readFloats(view, 32, 7),
    reverbParams: readFloats(view, 60, 7),
    fxEnabled: [raw[88], raw[89], raw[90]],
  };
}

export function buildRawData(preset: ParsedPreset): Uint8Array {
  const raw  = new Uint8Array(92);
  const view = new DataView(raw.buffer);
  raw[0] = 1; // valid
  raw[1] = preset.modMode;
  raw[2] = preset.delayMode;
  raw[3] = preset.reverbMode;
  const writeFloats = (offset: number, vals: number[]) =>
    vals.forEach((v, i) => view.setFloat32(offset + i * 4, Math.max(0, Math.min(1, v)), true));
  writeFloats(4,  preset.modParams);
  writeFloats(32, preset.delayParams);
  writeFloats(60, preset.reverbParams);
  raw[88] = preset.fxEnabled[0];
  raw[89] = preset.fxEnabled[1];
  raw[90] = preset.fxEnabled[2];
  return raw;
}
```

- [ ] **Step 2: Commit**

```bash
git add editor/src/lib/presetCodec.ts
git commit -m "feat: add preset struct codec (parse/build 92-byte MultiPresetSlot)"
```

---

## Task 2: useMidi — loadPreset, savePreset, ACK parsing

**Files:**
- Modify: `editor/src/hooks/useMidi.ts`

Key changes:
- Import `parsePresetSlot` and `ParsedPreset` from the codec.
- Add `presetsRef` (keeps presets accessible in async callbacks without stale closure).
- Add `loadPendingRef` and `savePendingRef` for Promise resolution.
- Add `loadPreset` and `savePreset` async actions.
- Update the `midi-sysex` listener to also handle ACK (cmd `0x83`) and resolve pending Promises.
- Keep existing `putPreset` (fire-and-forget, used by the import flow).
- Remove `setActivePreset` from the return (replaced by `loadPreset`).

The ACK frame from the device: `[0xF0, 0x7D, 0x83, originalCmd, ok, activeBank, activeSlot, 0xF7]`.
`ok === 0x00` means success; `ok === 0x01` means failure.

- [ ] **Step 1: Replace `editor/src/hooks/useMidi.ts` with the updated version**

```typescript
// editor/src/hooks/useMidi.ts
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState, useCallback } from "react";
import { parsePresetSlot, ParsedPreset } from "../lib/presetCodec";

const CC_MOD_BASE    = 14;
const CC_DELAY_BASE  = 21;
const CC_REVERB_BASE = 28;

export interface PresetData {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array; // 92 bytes
}

export interface LoadedPresetResult {
  modMode: number;
  delayMode: number;
  reverbMode: number;
  modParams: number[];
  delayParams: number[];
  reverbParams: number[];
  fxEnabled: [number, number, number];
  name: string;
}

interface LoadPending {
  resolve: (result: LoadedPresetResult | null) => void;
  timer: ReturnType<typeof setTimeout>;
}

interface SavePending {
  resolve: (ok: boolean) => void;
  timer: ReturnType<typeof setTimeout>;
}

export function useMidi() {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [presets, setPresets] = useState<(PresetData | null)[]>(
    Array(100).fill(null)
  );

  const ccThrottle   = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());
  const presetsRef   = useRef<(PresetData | null)[]>(Array(100).fill(null));
  const loadPending  = useRef<Map<string, LoadPending>>(new Map());
  const savePending  = useRef<SavePending | null>(null);

  // Keep presetsRef in sync so async callbacks see current data.
  useEffect(() => { presetsRef.current = presets; }, [presets]);

  const refreshPorts = useCallback(async () => {
    const p = await invoke<string[]>("list_midi_ports");
    setPorts(p);
  }, []);

  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    setConnected(true);
  }, []);

  const sendCC = useCallback(
    (stage: "mod" | "delay" | "reverb", paramIndex: number, normalised: number) => {
      const base =
        stage === "mod"   ? CC_MOD_BASE :
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

  const setMode = useCallback((stage: number, modeIndex: number) => {
    invoke("set_mode", { stage, modeIndex }).catch(console.error);
  }, []);

  const getAllPresets = useCallback(() => {
    invoke("get_all_presets").catch(console.error);
  }, []);

  // Fire-and-forget — used by the import flow.
  const putPreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array) => {
      invoke("put_preset", {
        bank, slot, name, rawData: Array.from(rawData),
      }).catch(console.error);
    },
    []
  );

  // Promise-based save; resolves true on success ACK, false on error/timeout.
  const savePreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array): Promise<boolean> => {
      if (savePending.current) {
        clearTimeout(savePending.current.timer);
        savePending.current.resolve(false);
        savePending.current = null;
      }
      return new Promise<boolean>((resolve) => {
        const timer = setTimeout(() => {
          savePending.current = null;
          resolve(false);
        }, 2000);
        savePending.current = { resolve, timer };
        invoke("put_preset", {
          bank, slot, name, rawData: Array.from(rawData),
        }).catch(() => {
          clearTimeout(timer);
          savePending.current = null;
          resolve(false);
        });
      });
    },
    []
  );

  // Sends SET_ACTIVE to device and resolves with parsed preset params + name.
  // For cached slots resolves immediately; for uncached slots fetches first (2 s timeout).
  const loadPreset = useCallback(
    (bank: number, slot: number): Promise<LoadedPresetResult | null> => {
      invoke("set_active_preset", { bank, slot }).catch(console.error);
      const idx    = bank * 10 + slot;
      const cached = presetsRef.current[idx];
      if (cached) {
        const parsed = parsePresetSlot(cached.rawData);
        return Promise.resolve(toResult(parsed, cached.name));
      }
      invoke("get_preset", { bank, slot }).catch(console.error);
      return new Promise<LoadedPresetResult | null>((resolve) => {
        const key   = `${bank}-${slot}`;
        const timer = setTimeout(() => {
          loadPending.current.delete(key);
          resolve(null);
        }, 2000);
        loadPending.current.set(key, { resolve, timer });
      });
    },
    []
  );

  // Listen for incoming SysEx from the device.
  useEffect(() => {
    const unlisten = listen<number[]>("midi-sysex", (event) => {
      const msg = new Uint8Array(event.payload);
      if (msg.length < 3 || msg[0] !== 0xF0 || msg[1] !== 0x7D) return;
      const cmd = msg[2];

      if (cmd === 0x81) {
        // PRESET_DATA: F0 7D 81 bank slot name[12] encoded[106] F7
        if (msg.length < 5) return;
        const bank      = msg[3];
        const slot      = msg[4];
        const nameBytes = msg.slice(5, 17);
        const name      = new TextDecoder().decode(nameBytes).replace(/\0+$/, "");
        const encoded   = msg.slice(17, msg.length - 1);
        const rawData   = decode7bit(encoded);
        const idx       = bank * 10 + slot;
        setPresets((prev) => {
          const next = [...prev];
          next[idx]  = { bank, slot, name, rawData };
          return next;
        });
        const key     = `${bank}-${slot}`;
        const pending = loadPending.current.get(key);
        if (pending) {
          clearTimeout(pending.timer);
          loadPending.current.delete(key);
          pending.resolve(toResult(parsePresetSlot(rawData), name));
        }
      } else if (cmd === 0x83) {
        // ACK: F0 7D 83 originalCmd ok bank slot F7
        if (msg.length < 6) return;
        const originalCmd = msg[3];
        const ok          = msg[4] === 0x00;
        if (originalCmd === 0x02 && savePending.current) {
          clearTimeout(savePending.current.timer);
          const { resolve } = savePending.current;
          savePending.current = null;
          resolve(ok);
        }
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
    setMode,
    getAllPresets,
    putPreset,
    savePreset,
    loadPreset,
  };
}

function toResult(parsed: ParsedPreset, name: string): LoadedPresetResult {
  return { ...parsed, name };
}

function decode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  let i = 0;
  while (i < input.length) {
    const remaining = input.length - i;
    if (remaining < 2) break;
    const chunkLen = Math.min(remaining - 1, 7);
    const msb      = input[i++];
    for (let j = 0; j < chunkLen; j++) {
      out.push(input[i++] | (((msb >> j) & 1) << 7));
    }
  }
  return new Uint8Array(out);
}
```

- [ ] **Step 2: Commit**

```bash
git add editor/src/hooks/useMidi.ts
git commit -m "feat: add loadPreset/savePreset with Promise-based ACK handling to useMidi"
```

---

## Task 3: PresetHeader component

**Files:**
- Create: `editor/src/components/PresetHeader.tsx`

Shows: connection dot + port selector (collapses when connected), editable preset name, bank/slot badge, amber dirty dot, Save button, ⋯ dropdown.

- [ ] **Step 1: Create the file**

```tsx
// editor/src/components/PresetHeader.tsx
import { useState, useRef, useEffect } from "react";
import { Button } from "@/components/ui/button";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";

interface PresetHeaderProps {
  connected: boolean;
  ports: string[];
  onConnect: (portName: string) => void;
  onRefresh: () => void;
  activePreset: { bank: number; slot: number } | null;
  presetName: string;
  isDirty: boolean;
  isSaving: boolean;
  saveError: string | null;
  onNameChange: (name: string) => void;
  onSave: () => void;
  onSyncAll: () => void;
  onExport: () => void;
  onImport: () => void;
}

export function PresetHeader({
  connected, ports, onConnect, onRefresh,
  activePreset, presetName, isDirty, isSaving, saveError,
  onNameChange, onSave, onSyncAll, onExport, onImport,
}: PresetHeaderProps) {
  const [selectedPort, setSelectedPort] = useState("");
  const [editing,      setEditing]      = useState(false);
  const [nameInput,    setNameInput]    = useState(presetName);
  const [menuOpen,     setMenuOpen]     = useState(false);
  const menuRef  = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (!editing) setNameInput(presetName);
  }, [presetName, editing]);

  useEffect(() => {
    if (!menuOpen) return;
    const handler = (e: MouseEvent) => {
      if (!menuRef.current?.contains(e.target as Node)) setMenuOpen(false);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [menuOpen]);

  const startEdit = () => {
    if (!activePreset) return;
    setEditing(true);
    setNameInput(presetName);
    setTimeout(() => inputRef.current?.select(), 0);
  };

  const commitEdit = () => {
    setEditing(false);
    onNameChange(nameInput.slice(0, 11).trim());
  };

  return (
    <div className="flex items-center gap-2 bg-zinc-900 border border-zinc-800 rounded-lg px-3 py-2">
      {/* Connection */}
      <div className="flex items-center gap-2 flex-shrink-0">
        <span className={`text-xs ${connected ? "text-cyan-400" : "text-zinc-500"}`}>⬤</span>
        {connected ? (
          <span className="text-xs text-zinc-400">Connected</span>
        ) : (
          <>
            <Select value={selectedPort} onValueChange={(v) => setSelectedPort(v ?? "")}>
              <SelectTrigger className="w-48 h-7 text-xs">
                <SelectValue placeholder="Select MIDI port" />
              </SelectTrigger>
              <SelectContent>
                {ports.map((p) => (
                  <SelectItem key={p} value={p}>{p}</SelectItem>
                ))}
              </SelectContent>
            </Select>
            <Button
              size="sm"
              className="h-7 text-xs px-2"
              disabled={!selectedPort}
              onClick={() => onConnect(selectedPort)}
            >
              Connect
            </Button>
            <Button size="sm" variant="outline" className="h-7 text-xs px-2" onClick={onRefresh}>
              ↺
            </Button>
          </>
        )}
      </div>

      <span className="text-zinc-700 mx-1 flex-shrink-0">|</span>

      {/* Preset identity */}
      <div className="flex items-center gap-2 flex-1 min-w-0">
        {editing ? (
          <input
            ref={inputRef}
            className="bg-zinc-800 border border-zinc-600 rounded px-2 py-0.5 text-sm text-zinc-100 w-36 focus:outline-none focus:border-cyan-500"
            value={nameInput}
            maxLength={11}
            onChange={(e) => setNameInput(e.target.value)}
            onBlur={commitEdit}
            onKeyDown={(e) => {
              if (e.key === "Enter")  commitEdit();
              if (e.key === "Escape") { setEditing(false); setNameInput(presetName); }
            }}
          />
        ) : (
          <span
            className={`text-sm font-medium truncate max-w-[9rem] ${
              activePreset
                ? "text-zinc-100 cursor-text hover:text-white"
                : "text-zinc-600 cursor-default"
            }`}
            title={activePreset ? "Click to rename" : undefined}
            onClick={startEdit}
          >
            {activePreset ? (presetName || "—") : "No preset loaded"}
          </span>
        )}
        {activePreset && (
          <span className="text-xs text-zinc-500 bg-zinc-800 border border-zinc-700 rounded px-1.5 py-0.5 flex-shrink-0">
            B{activePreset.bank}·{String(activePreset.slot).padStart(2, "0")}
          </span>
        )}
        {isDirty && (
          <span className="text-amber-400 text-sm flex-shrink-0" title="Unsaved changes">●</span>
        )}
        {saveError && (
          <span className="text-red-400 text-xs flex-shrink-0">{saveError}</span>
        )}
      </div>

      {/* Actions */}
      <div className="flex items-center gap-1.5 flex-shrink-0">
        <Button
          size="sm"
          className="h-7 text-xs px-3"
          disabled={!isDirty || !connected || !activePreset || isSaving}
          onClick={onSave}
        >
          {isSaving ? "Saving…" : "Save"}
        </Button>

        <div className="relative" ref={menuRef}>
          <Button
            size="sm"
            variant="outline"
            className="h-7 text-xs px-2"
            onClick={() => setMenuOpen((v) => !v)}
          >
            ⋯
          </Button>
          {menuOpen && (
            <div className="absolute right-0 top-full mt-1 bg-zinc-900 border border-zinc-700 rounded-md shadow-lg z-50 py-1 min-w-[8rem]">
              {[
                { label: "↺ Sync All", action: onSyncAll },
                { label: "↑ Export…", action: onExport },
                { label: "↓ Import…", action: onImport },
              ].map(({ label, action }) => (
                <button
                  key={label}
                  className="w-full text-left text-xs px-3 py-1.5 text-zinc-300 hover:bg-zinc-800"
                  onClick={() => { action(); setMenuOpen(false); }}
                >
                  {label}
                </button>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
```

- [ ] **Step 2: Commit**

```bash
git add editor/src/components/PresetHeader.tsx
git commit -m "feat: add PresetHeader component with connection, active preset, dirty indicator, Save, and ⋯ menu"
```

---

## Task 4: PresetBrowser — active highlight, collapse, dimmed null slots

**Files:**
- Modify: `editor/src/components/PresetBrowser.tsx`

Changes vs the current file:
- Remove `onSyncAll`, `onExport`, `onImport` props (moved to `⋯` menu in PresetHeader).
- Add `activePreset: { bank: number; slot: number } | null` prop.
- Replace `<Tabs>` with a simple row of bank buttons (removes the `tabs` shadcn import).
- Active bank follows `activePreset` when it changes.
- Active slot card: cyan border + faint background.
- Null slot card: `opacity-50` + italic dash.
- Collapse toggle at the top.

- [ ] **Step 1: Replace `editor/src/components/PresetBrowser.tsx`**

```tsx
// editor/src/components/PresetBrowser.tsx
import { useState, useEffect } from "react";
import { Card } from "@/components/ui/card";
import { PresetData } from "../hooks/useMidi";

interface PresetBrowserProps {
  presets: (PresetData | null)[];
  activePreset: { bank: number; slot: number } | null;
  onSelect: (bank: number, slot: number) => void;
}

export function PresetBrowser({ presets, activePreset, onSelect }: PresetBrowserProps) {
  const [isOpen,     setIsOpen]     = useState(true);
  const [activeBank, setActiveBank] = useState(activePreset?.bank ?? 0);

  // Follow active preset's bank when it changes.
  useEffect(() => {
    if (activePreset != null) setActiveBank(activePreset.bank);
  }, [activePreset?.bank, activePreset?.slot]);

  return (
    <div className="border-t border-zinc-800 pt-3">
      <button
        className="flex items-center gap-2 text-sm font-semibold text-zinc-400 uppercase tracking-wide mb-2 w-full text-left hover:text-zinc-300"
        onClick={() => setIsOpen((v) => !v)}
      >
        <span>{isOpen ? "▾" : "▸"}</span>
        <span>Presets — Bank {activeBank}</span>
      </button>

      {isOpen && (
        <>
          <div className="flex gap-1 mb-2 flex-wrap">
            {Array.from({ length: 10 }, (_, b) => (
              <button
                key={b}
                className={`text-xs px-2 py-1 rounded transition-colors ${
                  activeBank === b
                    ? "bg-zinc-700 text-zinc-100"
                    : "text-zinc-500 hover:text-zinc-300"
                }`}
                onClick={() => setActiveBank(b)}
              >
                Bank {b}
              </button>
            ))}
          </div>

          <div className="grid grid-cols-5 gap-2">
            {Array.from({ length: 10 }, (_, slot) => {
              const idx      = activeBank * 10 + slot;
              const data     = presets[idx];
              const isActive =
                activePreset?.bank === activeBank &&
                activePreset?.slot === slot;
              return (
                <Card
                  key={slot}
                  className={`p-2 cursor-pointer transition-colors ${
                    isActive
                      ? "bg-cyan-950 border-cyan-600"
                      : "hover:bg-zinc-800"
                  } ${!data ? "opacity-50" : ""}`}
                  onClick={() => onSelect(activeBank, slot)}
                >
                  <div className="text-[10px] text-zinc-500">
                    B{activeBank}·{String(slot).padStart(2, "0")}
                  </div>
                  <div
                    className={`text-xs truncate mt-0.5 ${
                      data ? "text-zinc-200" : "text-zinc-600 italic"
                    }`}
                  >
                    {data?.name || "—"}
                  </div>
                </Card>
              );
            })}
          </div>
        </>
      )}
    </div>
  );
}
```

- [ ] **Step 2: Commit**

```bash
git add editor/src/components/PresetBrowser.tsx
git commit -m "feat: update PresetBrowser with active highlight, collapse, dimmed null slots"
```

---

## Task 5: App.tsx — wire everything together

**Files:**
- Modify: `editor/src/App.tsx`

Changes:
- Import `PresetHeader`, `buildRawData`, `LoadedPresetResult`.
- Remove old `selectedPort` state and connection bar JSX.
- Add `activePreset`, `loadedSnapshot`, `presetName`, `isSaving`, `saveError` state.
- Derive `isDirty` (compare current params/modes/name vs `loadedSnapshot`, rounding to 2dp).
- Add `handlePresetSelect`, `handleSave`, `handleNameChange`, `handleConnect`.
- Pass new props to `PresetHeader` and `PresetBrowser`.
- `ExportDialog` still uses `midi.putPreset` — no change needed there.

- [ ] **Step 1: Replace `editor/src/App.tsx`**

```tsx
// editor/src/App.tsx
import { useState, useCallback, useEffect } from "react";
import { StageCard }      from "./components/StageCard";
import { PresetBrowser }  from "./components/PresetBrowser";
import { PresetHeader }   from "./components/PresetHeader";
import { ExportDialog }   from "./components/ExportDialog";
import { useMidi, PresetData, LoadedPresetResult } from "./hooks/useMidi";
import { buildRawData }   from "./lib/presetCodec";

const DEFAULT_PARAMS = Array(7).fill(0.5) as number[];
const round2 = (v: number) => Math.round(v * 100) / 100;

type LoadedSnapshot = LoadedPresetResult;

function paramsEqual(a: number[], b: number[]) {
  return a.every((v, i) => round2(v) === round2(b[i]));
}

export default function App() {
  const midi = useMidi();

  // Effect params / modes
  const [modMode,    setModMode]    = useState(0);
  const [delayMode,  setDelayMode]  = useState(0);
  const [reverbMode, setReverbMode] = useState(0);
  const [modParams,    setModParams]    = useState<number[]>([...DEFAULT_PARAMS]);
  const [delayParams,  setDelayParams]  = useState<number[]>([...DEFAULT_PARAMS]);
  const [reverbParams, setReverbParams] = useState<number[]>([...DEFAULT_PARAMS]);

  // Preset tracking
  const [activePreset,   setActivePreset]   = useState<{ bank: number; slot: number } | null>(null);
  const [loadedSnapshot, setLoadedSnapshot] = useState<LoadedSnapshot | null>(null);
  const [presetName,     setPresetName]     = useState("");
  const [isSaving,       setIsSaving]       = useState(false);
  const [saveError,      setSaveError]      = useState<string | null>(null);
  const [exportOpen,     setExportOpen]     = useState(false);

  useEffect(() => { midi.refreshPorts(); }, []);

  // Derived dirty flag
  const isDirty =
    activePreset !== null &&
    loadedSnapshot !== null &&
    (presetName !== loadedSnapshot.name ||
      modMode    !== loadedSnapshot.modMode    ||
      delayMode  !== loadedSnapshot.delayMode  ||
      reverbMode !== loadedSnapshot.reverbMode ||
      !paramsEqual(modParams,    loadedSnapshot.modParams)    ||
      !paramsEqual(delayParams,  loadedSnapshot.delayParams)  ||
      !paramsEqual(reverbParams, loadedSnapshot.reverbParams));

  const handleConnect = useCallback(async (portName: string) => {
    try { await midi.connect(portName); }
    catch (e) { console.error("MIDI connect failed:", e); }
  }, [midi]);

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

  const handlePresetSelect = useCallback(
    async (bank: number, slot: number) => {
      const result = await midi.loadPreset(bank, slot);
      if (!result) return;
      setModMode(result.modMode);
      setDelayMode(result.delayMode);
      setReverbMode(result.reverbMode);
      setModParams([...result.modParams]);
      setDelayParams([...result.delayParams]);
      setReverbParams([...result.reverbParams]);
      setPresetName(result.name);
      setActivePreset({ bank, slot });
      setLoadedSnapshot({ ...result });
      setSaveError(null);
    },
    [midi]
  );

  const handleSave = useCallback(async () => {
    if (!activePreset || !loadedSnapshot) return;
    setIsSaving(true);
    setSaveError(null);
    const rawData = buildRawData({
      modMode,
      delayMode,
      reverbMode,
      modParams,
      delayParams,
      reverbParams,
      fxEnabled: loadedSnapshot.fxEnabled,
    });
    const ok = await midi.savePreset(
      activePreset.bank, activePreset.slot, presetName, rawData
    );
    setIsSaving(false);
    if (ok) {
      setLoadedSnapshot({
        modMode, delayMode, reverbMode,
        modParams:    [...modParams],
        delayParams:  [...delayParams],
        reverbParams: [...reverbParams],
        fxEnabled:    loadedSnapshot.fxEnabled,
        name:         presetName,
      });
    } else {
      setSaveError("Save failed");
    }
  }, [activePreset, loadedSnapshot, modMode, delayMode, reverbMode,
      modParams, delayParams, reverbParams, presetName, midi]);

  const handleImportDone = useCallback(
    (imported: (PresetData | null)[]) => {
      imported.forEach((p) => {
        if (!p) return;
        midi.putPreset(p.bank, p.slot, p.name, p.rawData);
      });
    },
    [midi]
  );

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100 p-4 flex flex-col gap-4">
      <PresetHeader
        connected={midi.connected}
        ports={midi.ports}
        onConnect={handleConnect}
        onRefresh={midi.refreshPorts}
        activePreset={activePreset}
        presetName={presetName}
        isDirty={isDirty}
        isSaving={isSaving}
        saveError={saveError}
        onNameChange={setPresetName}
        onSave={handleSave}
        onSyncAll={midi.getAllPresets}
        onExport={() => setExportOpen(true)}
        onImport={() => setExportOpen(true)}
      />

      <div className="flex gap-4">
        <StageCard title="Mod"    stage="mod"    stageIndex={0} modeIndex={modMode}    params={modParams}    onParamChange={handleParamChange} onModeChange={handleModeChange} />
        <StageCard title="Delay"  stage="delay"  stageIndex={1} modeIndex={delayMode}  params={delayParams}  onParamChange={handleParamChange} onModeChange={handleModeChange} />
        <StageCard title="Reverb" stage="reverb" stageIndex={2} modeIndex={reverbMode} params={reverbParams} onParamChange={handleParamChange} onModeChange={handleModeChange} />
      </div>

      <PresetBrowser
        presets={midi.presets}
        activePreset={activePreset}
        onSelect={handlePresetSelect}
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

- [ ] **Step 2: Commit**

```bash
git add editor/src/App.tsx
git commit -m "feat: wire activePreset, loadedSnapshot, isDirty, handlePresetSelect, handleSave into App"
```

---

## Task 6: Verify in the app

- [ ] **Step 1: Start the dev server**

```bash
cd editor && npm run tauri dev
```

Expected: app compiles and opens without errors.

- [ ] **Step 2: Verify connection UI**

- No device connected: header shows grey dot + port dropdown + Connect button.
- Select a MIDI port and click Connect: dot turns cyan, port selector collapses to "Connected".

- [ ] **Step 3: Verify preset browser**

- Click **⋯ → Sync All** — all 100 preset slots fetch from device.
- Previously-synced slots show their names; unsynced slots show dim italic "—".
- Click a bank button — grid updates to show that bank's 10 slots.
- Click the **▾ Presets** toggle — browser collapses. Click again — expands.

- [ ] **Step 4: Verify preset load**

- Click a preset slot that has been synced (shows a name).
- Header updates: preset name and `B{bank}·{slot}` badge appear.
- Stage card knobs move to match the preset's saved values.
- Mode selectors update to the preset's saved modes.
- No dirty dot (●) is shown.

- [ ] **Step 5: Verify dirty detection**

- With a preset loaded, move any knob.
- Amber ● appears next to the preset name.
- Save button becomes enabled.

- [ ] **Step 6: Verify rename**

- Click the preset name text in the header.
- An input appears with the current name selected.
- Type a new name, press Enter.
- Amber ● appears (name differs from snapshot).

- [ ] **Step 7: Verify save**

- Click Save.
- Button shows "Saving…" briefly.
- On success: amber ● disappears, button disables.
- Preset browser updates the slot card to show the new name.

- [ ] **Step 8: Verify loading an unsynced slot (if any null slots remain)**

- Click a dim "—" slot.
- A GET_PRESET is sent; slot briefly dim until data arrives.
- On arrival: header, knobs, modes update; slot card shows the name.

- [ ] **Step 9: Final commit**

```bash
git add -A
git commit -m "chore: editor UX overhaul complete"
```
