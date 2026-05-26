import { useCallback, useEffect, useState } from "react";
import { Button } from "@/components/ui/button";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { ExportDialog } from "./components/ExportDialog";
import { PresetBrowser } from "./components/PresetBrowser";
import { StageCard } from "./components/StageCard";
import { PresetData, useMidi } from "./hooks/useMidi";

const DEFAULT_PARAMS = Array(7).fill(0.5) as number[];
const IMPORT_SEND_DELAY_MS = 75;

type Stage = "mod" | "delay" | "reverb";

export default function App() {
  const {
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
  } = useMidi();
  const [selectedPort, setSelectedPort] = useState("");
  const [modMode, setModMode] = useState(0);
  const [delayMode, setDelayMode] = useState(0);
  const [reverbMode, setReverbMode] = useState(0);
  const [modParams, setModParams] = useState<number[]>([...DEFAULT_PARAMS]);
  const [delayParams, setDelayParams] = useState<number[]>([...DEFAULT_PARAMS]);
  const [reverbParams, setReverbParams] = useState<number[]>([...DEFAULT_PARAMS]);
  const [exportOpen, setExportOpen] = useState(false);
  const [importing, setImporting] = useState(false);

  useEffect(() => {
    void refreshPorts();
  }, [refreshPorts]);

  const handleConnect = async () => {
    if (selectedPort) {
      await connect(selectedPort);
    }
  };

  const handleParamChange = useCallback(
    (stage: Stage, index: number, value: number) => {
      const update = (params: number[]) => params.map((current, i) => (i === index ? value : current));
      if (stage === "mod") setModParams(update);
      if (stage === "delay") setDelayParams(update);
      if (stage === "reverb") setReverbParams(update);
      sendCC(stage, index, value);
    },
    [sendCC],
  );

  const handleModeChange = useCallback(
    (stage: Stage, index: number) => {
      if (stage === "mod") setModMode(index);
      if (stage === "delay") setDelayMode(index);
      if (stage === "reverb") setReverbMode(index);
      void setMode(stage === "mod" ? 0 : stage === "delay" ? 1 : 2, index);
    },
    [setMode],
  );

  const handleImportDone = useCallback(
    async (imported: (PresetData | null)[]) => {
      setImporting(true);
      try {
        for (const preset of imported) {
          if (!preset) continue;
          await putPreset(preset.bank, preset.slot, preset.name, preset.rawData);
          await new Promise((resolve) => setTimeout(resolve, IMPORT_SEND_DELAY_MS));
        }
      } finally {
        setImporting(false);
      }
    },
    [putPreset],
  );

  return (
    <main className="min-h-screen overflow-hidden bg-[radial-gradient(circle_at_top_left,#27272a_0,#09090b_42%,#050505_100%)] p-4 text-zinc-100">
      <div className="mx-auto flex max-w-7xl flex-col gap-4">
        <header className="flex flex-col gap-3 rounded-2xl border border-zinc-800 bg-black/30 p-4 shadow-2xl shadow-black/30 backdrop-blur md:flex-row md:items-center md:justify-between">
          <div>
            <h1 className="text-xl font-semibold tracking-tight">Multi-FX Editor</h1>
            <p className="text-sm text-zinc-500">USB MIDI control, SysEx preset sync, and 100-slot librarian.</p>
          </div>
          <div className="flex flex-wrap items-center gap-2">
            <Select value={selectedPort} onValueChange={setSelectedPort}>
              <SelectTrigger className="w-72">
                <SelectValue placeholder="Select MIDI port" />
              </SelectTrigger>
              <SelectContent>
                {ports.map((port) => (
                  <SelectItem key={port} value={port}>
                    {port}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
            <Button onClick={handleConnect} disabled={!selectedPort || connected}>
              {connected ? "Connected" : "Connect"}
            </Button>
            <Button variant="outline" onClick={refreshPorts}>
              Refresh
            </Button>
          </div>
        </header>

        <section className="grid gap-4 lg:grid-cols-3">
          <StageCard title="Mod" stage="mod" modeIndex={modMode} params={modParams} onParamChange={handleParamChange} onModeChange={handleModeChange} />
          <StageCard title="Delay" stage="delay" modeIndex={delayMode} params={delayParams} onParamChange={handleParamChange} onModeChange={handleModeChange} />
          <StageCard title="Reverb" stage="reverb" modeIndex={reverbMode} params={reverbParams} onParamChange={handleParamChange} onModeChange={handleModeChange} />
        </section>

        <PresetBrowser
          presets={presets}
          onSelect={(bank, slot) => void setActivePreset(bank, slot)}
          onSyncAll={() => void getAllPresets()}
          onExport={() => setExportOpen(true)}
          onImport={() => setExportOpen(true)}
        />

        {importing ? <p className="text-xs text-zinc-500">Uploading imported presets to device...</p> : null}
      </div>

      <ExportDialog open={exportOpen} onClose={() => setExportOpen(false)} presets={presets} onImportDone={handleImportDone} />
    </main>
  );
}
