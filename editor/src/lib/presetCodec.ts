export interface ParsedPreset {
  valid: number;
  modMode: number;
  delayMode: number;
  reverbMode: number;
  modParams: number[];              // 7 normalised [0,1]
  delayParams: number[];            // 7 normalised [0,1]
  reverbParams: number[];           // 7 normalised [0,1]
  fxEnabled: [number, number, number]; // preserved for round-trip
}

function readFloats(view: DataView, offset: number, count: number): number[] {
  return Array.from({ length: count }, (_, i) =>
    Math.max(0, Math.min(1, view.getFloat32(offset + i * 4, true)))
  );
}

export function parsePresetSlot(raw: Uint8Array): ParsedPreset {
  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  return {
    valid:      raw[0],
    modMode:    raw[1],
    delayMode:  raw[2],
    reverbMode: raw[3],
    modParams:    readFloats(view, 4,  7),
    delayParams:  readFloats(view, 32, 7),
    reverbParams: readFloats(view, 60, 7),
    fxEnabled: [raw[88], raw[89], raw[90]],
  };
}

export function buildRawData(preset: ParsedPreset): Uint8Array {
  const raw  = new Uint8Array(92);
  const view = new DataView(raw.buffer);
  raw[0] = 1; // valid
  raw[1] = preset.modMode;
  raw[2] = preset.delayMode;
  raw[3] = preset.reverbMode;
  const writeFloats = (offset: number, vals: number[]) =>
    vals.forEach((v, i) => view.setFloat32(offset + i * 4, Math.max(0, Math.min(1, v)), true));
  writeFloats(4,  preset.modParams);
  writeFloats(32, preset.delayParams);
  writeFloats(60, preset.reverbParams);
  raw[88] = preset.fxEnabled[0];
  raw[89] = preset.fxEnabled[1];
  raw[90] = preset.fxEnabled[2];
  return raw;
}
