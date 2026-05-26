import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { KnobPanel } from "./KnobPanel";
import { ModeSelector } from "./ModeSelector";

interface StageCardProps {
  title: string;
  stage: "mod" | "delay" | "reverb";
  modeIndex: number;
  params: number[];
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
}

export function StageCard({ title, stage, modeIndex, params, onParamChange, onModeChange }: StageCardProps) {
  return (
    <Card className="min-w-0 flex-1 border-zinc-800 bg-zinc-950/85">
      <CardHeader className="gap-3 pb-2">
        <CardTitle className="text-sm font-semibold uppercase tracking-[0.24em] text-zinc-400">{title}</CardTitle>
        <ModeSelector stage={stage} modeIndex={modeIndex} onModeChange={onModeChange} />
      </CardHeader>
      <CardContent className="pt-0">
        <KnobPanel stage={stage} values={params} onParamChange={onParamChange} />
      </CardContent>
    </Card>
  );
}
