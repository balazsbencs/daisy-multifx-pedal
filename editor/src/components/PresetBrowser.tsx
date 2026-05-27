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
