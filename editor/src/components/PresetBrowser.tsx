import { Button } from "@/components/ui/button";
import { Card } from "@/components/ui/card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { PresetData } from "../hooks/useMidi";

interface PresetBrowserProps {
  presets: (PresetData | null)[];
  onSelect: (bank: number, slot: number) => void;
  onSyncAll: () => void;
  onExport: () => void;
  onImport: () => void;
}

export function PresetBrowser({ presets, onSelect, onSyncAll, onExport, onImport }: PresetBrowserProps) {
  return (
    <section className="rounded-2xl border border-zinc-800 bg-zinc-950/70 p-4">
      <div className="mb-3 flex items-center justify-between gap-3">
        <span className="text-sm font-semibold uppercase tracking-[0.24em] text-zinc-400">Presets</span>
        <div className="flex flex-wrap gap-2">
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
        <TabsList className="mb-3 flex h-auto flex-wrap justify-start">
          {Array.from({ length: 10 }, (_, bank) => (
            <TabsTrigger key={bank} value={String(bank)}>
              Bank {bank}
            </TabsTrigger>
          ))}
        </TabsList>

        {Array.from({ length: 10 }, (_, bank) => (
          <TabsContent key={bank} value={String(bank)}>
            <div className="grid grid-cols-2 gap-2 sm:grid-cols-5 lg:grid-cols-10">
              {Array.from({ length: 10 }, (_, slot) => {
                const data = presets[bank * 10 + slot];
                return (
                  <Card
                    key={slot}
                    className="cursor-pointer border-zinc-800 bg-zinc-900/70 p-3 transition hover:-translate-y-0.5 hover:bg-zinc-800"
                    onClick={() => onSelect(bank, slot)}
                  >
                    <div className="text-[10px] uppercase tracking-widest text-zinc-500">
                      B{bank}.{String(slot).padStart(2, "0")}
                    </div>
                    <div className="mt-1 truncate text-xs text-zinc-100">{data?.name || "Empty"}</div>
                  </Card>
                );
              })}
            </div>
          </TabsContent>
        ))}
      </Tabs>
    </section>
  );
}
