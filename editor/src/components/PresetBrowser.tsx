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
