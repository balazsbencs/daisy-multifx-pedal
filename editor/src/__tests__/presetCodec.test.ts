import { describe, it, expect } from "vitest";
import { parsePresetSlot, buildRawData } from "../lib/presetCodec";

// ── helpers ───────────────────────────────────────────────────────────────────

function makeRaw(valid: number, modMode = 1, delayMode = 2, reverbMode = 3): Uint8Array {
  const raw = new Uint8Array(92);
  const view = new DataView(raw.buffer);
  raw[0] = valid;
  raw[1] = modMode;
  raw[2] = delayMode;
  raw[3] = reverbMode;
  // Write some non-default float values to params
  for (let i = 0; i < 7; i++) view.setFloat32(4  + i * 4, 0.1 + i * 0.1, true); // mod
  for (let i = 0; i < 7; i++) view.setFloat32(32 + i * 4, 0.2 + i * 0.1, true); // delay
  for (let i = 0; i < 7; i++) view.setFloat32(60 + i * 4, 0.3 + i * 0.1, true); // reverb
  raw[88] = 1; raw[89] = 0; raw[90] = 1; // fx enabled
  return raw;
}

// ── parsePresetSlot ───────────────────────────────────────────────────────────

describe("parsePresetSlot", () => {
  it("extracts valid=1 from a populated slot", () => {
    const parsed = parsePresetSlot(makeRaw(1));
    expect(parsed.valid).toBe(1);
  });

  it("extracts valid=0 from an empty slot", () => {
    const parsed = parsePresetSlot(new Uint8Array(92));
    expect(parsed.valid).toBe(0);
  });

  it("extracts mode bytes correctly", () => {
    const parsed = parsePresetSlot(makeRaw(1, 3, 5, 7));
    expect(parsed.modMode).toBe(3);
    expect(parsed.delayMode).toBe(5);
    expect(parsed.reverbMode).toBe(7);
  });

  it("extracts fxEnabled triplet", () => {
    const parsed = parsePresetSlot(makeRaw(1));
    expect(parsed.fxEnabled).toEqual([1, 0, 1]);
  });

  it("clamps params to [0,1]", () => {
    const raw = new Uint8Array(92);
    const view = new DataView(raw.buffer);
    raw[0] = 1;
    view.setFloat32(4, -99.0, true);  // below range
    view.setFloat32(8, 999.0, true);  // above range
    const parsed = parsePresetSlot(raw);
    expect(parsed.modParams[0]).toBe(0);
    expect(parsed.modParams[1]).toBe(1);
  });
});

// ── buildRawData ──────────────────────────────────────────────────────────────

describe("buildRawData", () => {
  it("always writes valid=1 in byte 0", () => {
    const raw = buildRawData({
      valid: 0,  // input ignored — output is always 1
      modMode: 0, delayMode: 0, reverbMode: 0,
      modParams: Array(7).fill(0.5),
      delayParams: Array(7).fill(0.5),
      reverbParams: Array(7).fill(0.5),
      fxEnabled: [1, 0, 0],
    });
    expect(raw[0]).toBe(1);
  });

  it("round-trips through parsePresetSlot", () => {
    const original = parsePresetSlot(makeRaw(1, 2, 4, 6));
    const rebuilt  = buildRawData(original);
    const parsed   = parsePresetSlot(rebuilt);
    expect(parsed.valid).toBe(1);
    expect(parsed.modMode).toBe(2);
    expect(parsed.delayMode).toBe(4);
    expect(parsed.reverbMode).toBe(6);
    original.modParams.forEach((v, i) =>
      expect(Math.abs(parsed.modParams[i] - v)).toBeLessThan(1e-5)
    );
    expect(parsed.fxEnabled).toEqual(original.fxEnabled);
  });
});

// ── isDirty logic ─────────────────────────────────────────────────────────────
// Extracted as a pure function to test without React.

interface Snapshot {
  name: string; modMode: number; delayMode: number; reverbMode: number;
  modParams: number[]; delayParams: number[]; reverbParams: number[];
  fxEnabled: [number, number, number];
}

function computeIsDirty(
  activePreset: { bank: number; slot: number } | null,
  loadedSnapshot: Snapshot | null,
  presetName: string, modMode: number, delayMode: number, reverbMode: number,
  modParams: number[], delayParams: number[], reverbParams: number[],
  fxEnabled: [boolean, boolean, boolean],
): boolean {
  const round2 = (v: number) => Math.round(v * 100) / 100;
  const paramsEqual = (a: number[], b: number[]) => a.every((v, i) => round2(v) === round2(b[i]));
  return (
    activePreset !== null &&
    (loadedSnapshot === null ||
      presetName !== loadedSnapshot.name ||
      modMode    !== loadedSnapshot.modMode    ||
      delayMode  !== loadedSnapshot.delayMode  ||
      reverbMode !== loadedSnapshot.reverbMode ||
      !paramsEqual(modParams,    loadedSnapshot.modParams)    ||
      !paramsEqual(delayParams,  loadedSnapshot.delayParams)  ||
      !paramsEqual(reverbParams, loadedSnapshot.reverbParams) ||
      fxEnabled.some((v, i) => v !== (loadedSnapshot.fxEnabled[i] !== 0)))
  );
}

const DEFAULT_PARAMS = Array(7).fill(0.5);
const DEFAULT_FX: [boolean, boolean, boolean] = [true, false, false];

describe("isDirty", () => {
  it("is false when activePreset is null (nothing loaded)", () => {
    expect(computeIsDirty(null, null, "", 0, 0, 0, DEFAULT_PARAMS, DEFAULT_PARAMS, DEFAULT_PARAMS, DEFAULT_FX)).toBe(false);
  });

  it("is TRUE when activePreset is set but loadedSnapshot is null (key bug fix)", () => {
    // This is the scenario after connect+liveState with NO setLoadedSnapshot call.
    // Before the fix, liveState set loadedSnapshot → isDirty=false, Save disabled.
    expect(computeIsDirty(
      { bank: 0, slot: 0 }, null,
      "", 0, 0, 0, DEFAULT_PARAMS, DEFAULT_PARAMS, DEFAULT_PARAMS, DEFAULT_FX
    )).toBe(true);
  });

  it("is false when params match snapshot exactly", () => {
    const snap: Snapshot = {
      name: "Test", modMode: 1, delayMode: 2, reverbMode: 3,
      modParams: [...DEFAULT_PARAMS], delayParams: [...DEFAULT_PARAMS], reverbParams: [...DEFAULT_PARAMS],
      fxEnabled: [1, 0, 0],
    };
    expect(computeIsDirty(
      { bank: 0, slot: 0 }, snap,
      "Test", 1, 2, 3, [...DEFAULT_PARAMS], [...DEFAULT_PARAMS], [...DEFAULT_PARAMS],
      [true, false, false]
    )).toBe(false);
  });

  it("is true when a param changes from snapshot", () => {
    const snap: Snapshot = {
      name: "Test", modMode: 1, delayMode: 2, reverbMode: 3,
      modParams: [...DEFAULT_PARAMS], delayParams: [...DEFAULT_PARAMS], reverbParams: [...DEFAULT_PARAMS],
      fxEnabled: [1, 0, 0],
    };
    const changedParams = [...DEFAULT_PARAMS];
    changedParams[0] = 0.9; // user moved a knob
    expect(computeIsDirty(
      { bank: 0, slot: 0 }, snap,
      "Test", 1, 2, 3, changedParams, [...DEFAULT_PARAMS], [...DEFAULT_PARAMS],
      [true, false, false]
    )).toBe(true);
  });

  it("is true when only the name changes", () => {
    const snap: Snapshot = {
      name: "Old", modMode: 0, delayMode: 0, reverbMode: 0,
      modParams: [...DEFAULT_PARAMS], delayParams: [...DEFAULT_PARAMS], reverbParams: [...DEFAULT_PARAMS],
      fxEnabled: [1, 0, 0],
    };
    expect(computeIsDirty(
      { bank: 0, slot: 0 }, snap,
      "New", 0, 0, 0, [...DEFAULT_PARAMS], [...DEFAULT_PARAMS], [...DEFAULT_PARAMS],
      [true, false, false]
    )).toBe(true);
  });
});
