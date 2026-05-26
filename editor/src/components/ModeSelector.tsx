import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";

const MOD_MODES    = ["Chorus","Flanger","Phaser","Tremolo","Vibrato","Auto-Wah"];
const DELAY_MODES  = ["Tape","Digital","Ping-Pong","Reverse","Slapback","Dotted-8th","LoFi","Shimmer","Ducking","Multi"];
const REVERB_MODES = ["Hall","Room","Plate","Spring","Cloud","Shimmer","Magneto","Gated","Freeze","Air","Chapel","Cave"];

const MODE_LISTS: Record<string, string[]> = {
  mod: MOD_MODES,
  delay: DELAY_MODES,
  reverb: REVERB_MODES,
};

interface ModeSelectorProps {
  stage: "mod" | "delay" | "reverb";
  modeIndex: number;
  onModeChange: (stage: "mod" | "delay" | "reverb", index: number) => void;
}

export function ModeSelector({ stage, modeIndex, onModeChange }: ModeSelectorProps) {
  const modes = MODE_LISTS[stage];
  return (
    <Select
      value={String(modeIndex)}
      onValueChange={(v) => onModeChange(stage, Number(v))}
    >
      <SelectTrigger className="w-full">
        <SelectValue placeholder="Select mode" />
      </SelectTrigger>
      <SelectContent>
        {modes.map((name, i) => (
          <SelectItem key={i} value={String(i)}>
            {name}
          </SelectItem>
        ))}
      </SelectContent>
    </Select>
  );
}
