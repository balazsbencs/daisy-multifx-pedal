import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { KnobPanel } from "./KnobPanel";
import { ModeSelector } from "./ModeSelector";

interface StageCardProps {
  title: string;
  stage: "mod" | "delay" | "reverb";
  stageIndex: number;          // 0=mod 1=delay 2=reverb (for SET_MODE)
  modeIndex: number;
  params: number[];            // 7 normalised values
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
}

export function StageCard({
  title,
  stage,
  modeIndex,
  params,
  onParamChange,
  onModeChange,
}: StageCardProps) {
  return (
    <Card className="flex-1 min-w-0">
      <CardHeader className="pb-2">
        <CardTitle className="text-sm font-semibold uppercase tracking-wide text-zinc-400">
          {title}
        </CardTitle>
        <ModeSelector stage={stage} modeIndex={modeIndex} onModeChange={onModeChange} />
      </CardHeader>
      <CardContent className="pt-0">
        <KnobPanel stage={stage} values={params} onParamChange={onParamChange} />
      </CardContent>
    </Card>
  );
}
