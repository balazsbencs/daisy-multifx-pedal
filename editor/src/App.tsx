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
        <Select value={selectedPort} onValueChange={(v) => setSelectedPort(v ?? "")}>
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
