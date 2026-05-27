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
  midiError: string | null;
  onNameChange: (name: string) => void;
  onSave: () => void;
  onSyncAll: () => void;
  onExport: () => void;
  onImport: () => void;
}

export function PresetHeader({
  connected, ports, onConnect, onRefresh,
  activePreset, presetName, isDirty, isSaving, saveError, midiError,
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
        {midiError && (
          <span className="text-orange-400 text-xs flex-shrink-0 max-w-[12rem] truncate" title={midiError}>
            ⚠ {midiError}
          </span>
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
                { label: "↑ Export…",  action: onExport  },
                { label: "↓ Import…",  action: onImport  },
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
