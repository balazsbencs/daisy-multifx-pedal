import { KnobHeadless, KnobHeadlessOutput } from "react-knob-headless";

const PARAM_NAMES = ["Speed/Time", "Depth/Repeats", "Mix", "Tone/Filter", "P1/Grit", "P2/ModSpd", "Level/ModDep"];

interface KnobPanelProps {
  stage: "mod" | "delay" | "reverb";
  values: number[];
  onParamChange: (stage: "mod" | "delay" | "reverb", index: number, value: number) => void;
}

export function KnobPanel({ stage, values, onParamChange }: KnobPanelProps) {
  return (
    <div className="grid grid-cols-4 gap-3 p-2">
      {values.slice(0, 7).map((value, index) => {
        const percentage = Math.max(0, Math.min(1, value));
        const dash = percentage * 75.4;

        return (
          <div key={PARAM_NAMES[index]} className="flex flex-col items-center gap-1">
            <KnobHeadless
              aria-label={PARAM_NAMES[index]}
              className="h-14 w-14 cursor-pointer rounded-full border border-zinc-700 bg-zinc-900 shadow-inner focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-zinc-300"
              valueRaw={value}
              valueMin={0}
              valueMax={1}
              dragSensitivity={0.006}
              valueRawRoundFn={(next) => Math.round(next * 100) / 100}
              valueRawDisplayFn={(next) => `${Math.round(next * 100)}`}
              onValueRawChange={(next: number) => onParamChange(stage, index, next)}
            >
              <svg viewBox="0 0 40 40" className="h-full w-full">
                <circle cx="20" cy="20" r="16" fill="none" stroke="#27272a" strokeWidth="4" />
                <circle
                  cx="20"
                  cy="20"
                  r="16"
                  fill="none"
                  stroke="#d4d4d8"
                  strokeDasharray={`${dash} 100`}
                  strokeLinecap="round"
                  strokeWidth="4"
                  transform="rotate(-220 20 20)"
                />
                <line
                  x1="20"
                  y1="20"
                  x2="20"
                  y2="8"
                  stroke="#fafafa"
                  strokeLinecap="round"
                  strokeWidth="2"
                  transform={`rotate(${percentage * 280 - 140} 20 20)`}
                />
                <circle cx="20" cy="20" r="3" fill="#fafafa" />
              </svg>
            </KnobHeadless>
            <KnobHeadlessOutput
              aria-live="off"
              className="w-14 truncate text-center text-xs text-zinc-300"
              htmlFor={PARAM_NAMES[index]}
            >
              {(value * 100).toFixed(0)}
            </KnobHeadlessOutput>
            <span className="w-16 truncate text-center text-[10px] text-zinc-500">{PARAM_NAMES[index]}</span>
          </div>
        );
      })}
    </div>
  );
}
