import { useState, useCallback, useEffect } from "react";
import { StageCard }     from "./components/StageCard";
import { PresetBrowser } from "./components/PresetBrowser";
import { PresetHeader }  from "./components/PresetHeader";
import { ExportDialog }  from "./components/ExportDialog";
import { useMidi, PresetData, LoadedPresetResult } from "./hooks/useMidi";
import { buildRawData }  from "./lib/presetCodec";

const DEFAULT_PARAMS  = Array(7).fill(0.5) as number[];
const DEFAULT_FX: [boolean, boolean, boolean] = [true, false, false];
const round2 = (v: number) => Math.round(v * 100) / 100;

type LoadedSnapshot = LoadedPresetResult;

function paramsEqual(a: number[], b: number[]) {
  return a.every((v, i) => round2(v) === round2(b[i]));
}

export default function App() {
  const midi = useMidi();

  const [modMode,    setModMode]    = useState(0);
  const [delayMode,  setDelayMode]  = useState(0);
  const [reverbMode, setReverbMode] = useState(0);
  const [modParams,    setModParams]    = useState<number[]>([...DEFAULT_PARAMS]);
  const [delayParams,  setDelayParams]  = useState<number[]>([...DEFAULT_PARAMS]);
  const [reverbParams, setReverbParams] = useState<number[]>([...DEFAULT_PARAMS]);
  const [fxEnabled,    setFxEnabled]    = useState<[boolean, boolean, boolean]>([...DEFAULT_FX]);

  const [activePreset,   setActivePreset]   = useState<{ bank: number; slot: number } | null>(null);
  const [loadedSnapshot, setLoadedSnapshot] = useState<LoadedSnapshot | null>(null);
  const [presetName,     setPresetName]     = useState("");
  const [isSaving,       setIsSaving]       = useState(false);
  const [saveError,      setSaveError]      = useState<string | null>(null);
  const [exportOpen,     setExportOpen]     = useState(false);

  useEffect(() => { midi.refreshPorts(); }, []);

  const isDirty =
    activePreset !== null &&
    loadedSnapshot !== null &&
    (presetName !== loadedSnapshot.name ||
      modMode    !== loadedSnapshot.modMode    ||
      delayMode  !== loadedSnapshot.delayMode  ||
      reverbMode !== loadedSnapshot.reverbMode ||
      !paramsEqual(modParams,    loadedSnapshot.modParams)    ||
      !paramsEqual(delayParams,  loadedSnapshot.delayParams)  ||
      !paramsEqual(reverbParams, loadedSnapshot.reverbParams) ||
      fxEnabled.some((v, i) => v !== (loadedSnapshot.fxEnabled[i] !== 0)));

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

  const handleFxToggle = useCallback(
    (stage: "mod" | "delay" | "reverb") => {
      const stageIndex = stage === "mod" ? 0 : stage === "delay" ? 1 : 2;
      setFxEnabled(prev => {
        const next: [boolean, boolean, boolean] = [...prev] as [boolean, boolean, boolean];
        next[stageIndex] = !prev[stageIndex];
        midi.setFxEnabled(stageIndex, next[stageIndex]);
        return next;
      });
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
      setFxEnabled(result.fxEnabled.map(v => v !== 0) as [boolean, boolean, boolean]);
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
    const fxRaw = fxEnabled.map(v => v ? 1 : 0) as [number, number, number];
    const rawData = buildRawData({
      modMode,
      delayMode,
      reverbMode,
      modParams,
      delayParams,
      reverbParams,
      fxEnabled: fxRaw,
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
        fxEnabled:    fxRaw,
        name:         presetName,
      });
    } else {
      setSaveError("Save failed");
    }
  }, [activePreset, loadedSnapshot, modMode, delayMode, reverbMode,
      modParams, delayParams, reverbParams, fxEnabled, presetName, midi]);

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
        midiError={midi.midiError}
        onNameChange={setPresetName}
        onSave={handleSave}
        onSyncAll={midi.getAllPresets}
        onExport={() => setExportOpen(true)}
        onImport={() => setExportOpen(true)}
      />

      <div className="flex gap-4">
        <StageCard title="Mod"    stage="mod"    stageIndex={0} modeIndex={modMode}    params={modParams}    fxEnabled={fxEnabled[0]} onParamChange={handleParamChange} onModeChange={handleModeChange} onFxToggle={handleFxToggle} />
        <StageCard title="Delay"  stage="delay"  stageIndex={1} modeIndex={delayMode}  params={delayParams}  fxEnabled={fxEnabled[1]} onParamChange={handleParamChange} onModeChange={handleModeChange} onFxToggle={handleFxToggle} />
        <StageCard title="Reverb" stage="reverb" stageIndex={2} modeIndex={reverbMode} params={reverbParams} fxEnabled={fxEnabled[2]} onParamChange={handleParamChange} onModeChange={handleModeChange} onFxToggle={handleFxToggle} />
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
