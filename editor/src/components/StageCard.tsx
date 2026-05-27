import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { KnobPanel } from "./KnobPanel";
import { ModeSelector } from "./ModeSelector";

const STAGE_COLOR: Record<string, string> = {
  mod:    "border-t-cyan-500",
  delay:  "border-t-amber-500",
  reverb: "border-t-violet-400",
};

interface StageCardProps {
  title: string;
  stage: "mod" | "delay" | "reverb";
  stageIndex: number;
  modeIndex: number;
  params: number[];
  fxEnabled: boolean;
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
  onFxToggle: (stage: "mod" | "delay" | "reverb") => void;
}

export function StageCard({
  title,
  stage,
  modeIndex,
  params,
  fxEnabled,
  onParamChange,
  onModeChange,
  onFxToggle,
}: StageCardProps) {
  return (
    <Card className={`flex-1 min-w-0 border-t-2 ${STAGE_COLOR[stage]}`}>
      <CardHeader className="pb-2">
        <div className="flex items-center justify-between">
          <CardTitle className="text-sm font-semibold uppercase tracking-wide text-zinc-400">
            {title}
          </CardTitle>
          <button
            onClick={() => onFxToggle(stage)}
            className={`text-xs px-2 py-0.5 rounded border transition-colors ${
              fxEnabled
                ? "bg-zinc-800 border-zinc-600 text-zinc-200"
                : "bg-zinc-950 border-zinc-700 text-zinc-600 line-through"
            }`}
            title={fxEnabled ? "Effect on — click to bypass" : "Effect bypassed — click to enable"}
          >
            {fxEnabled ? "ON" : "OFF"}
          </button>
        </div>
        <ModeSelector stage={stage} modeIndex={modeIndex} onModeChange={onModeChange} />
      </CardHeader>
      <CardContent className={`pt-0 ${!fxEnabled ? "opacity-40" : ""}`}>
        <KnobPanel stage={stage} values={params} onParamChange={onParamChange} />
      </CardContent>
    </Card>
  );
}
