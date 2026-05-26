import { useCallback } from "react";
import { KnobHeadless, KnobHeadlessOutput } from "react-knob-headless";

const PARAM_NAMES = ["Speed", "Depth", "Mix", "Tone", "Param 1", "Param 2", "Level"];

const ARC_COLOR: Record<string, string> = {
  mod:    "#22d3ee", // cyan
  delay:  "#f59e0b", // amber
  reverb: "#a78bfa", // violet
};

interface KnobPanelProps {
  stage: "mod" | "delay" | "reverb";
  values: number[]; // 7 normalised [0,1] values
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
}

const roundFn = (v: number) => Math.round(v * 100) / 100;
const displayFn = (v: number) => (v * 100).toFixed(0);

export function KnobPanel({ stage, values, onParamChange }: KnobPanelProps) {
  const arcColor = ARC_COLOR[stage];

  const handleChange = useCallback(
    (index: number, value: number) => {
      onParamChange(stage, index, value);
    },
    [stage, onParamChange]
  );

  return (
    <div className="grid grid-cols-4 gap-x-2 gap-y-4 p-2">
      {values.slice(0, 7).map((value, i) => {
        const knobId = `knob-${stage}-${i}`;
        const outputId = `output-${stage}-${i}`;
        const pct = Math.max(0, Math.min(1, value));
        const arcLen = pct * 75.4;

        return (
          <div key={i} className="flex flex-col items-center gap-1">
            <KnobHeadless
              id={knobId}
              aria-label={PARAM_NAMES[i]}
              valueRaw={value}
              valueMin={0}
              valueMax={1}
              dragSensitivity={0.006}
              valueRawRoundFn={roundFn}
              valueRawDisplayFn={displayFn}
              className="w-11 h-11 rounded-full bg-zinc-800 border border-zinc-600 cursor-pointer"
              onValueRawChange={(v) => handleChange(i, v)}
            >
              <svg viewBox="0 0 40 40" className="w-full h-full pointer-events-none">
                <circle cx="20" cy="20" r="16" fill="none" stroke="#3f3f46" strokeWidth="4" />
                <circle
                  cx="20"
                  cy="20"
                  r="16"
                  fill="none"
                  stroke={arcColor}
                  strokeWidth="4"
                  strokeDasharray={`${arcLen} 100`}
                  strokeLinecap="round"
                  transform="rotate(-220 20 20)"
                />
                <circle cx="20" cy="20" r="4" fill="#e4e4e7" />
              </svg>
            </KnobHeadless>
            <KnobHeadlessOutput
              id={outputId}
              htmlFor={knobId}
              className="text-xs text-zinc-400 text-center w-full"
            >
              {displayFn(value)}
            </KnobHeadlessOutput>
            <span className="text-[10px] text-zinc-500 text-center leading-tight w-full">
              {PARAM_NAMES[i]}
            </span>
          </div>
        );
      })}
    </div>
  );
}
