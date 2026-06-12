import { describe, it, expect } from "vitest";
import {
  encode7bit,
  decode7bit,
  parseAckFrame,
  parsePresetDataFrame,
  parseLiveStateFrame,
  normalisedToCC,
  ccNumber,
  ccToMapping,
  RAW_PRESET_BYTES,
  ENCODED_PRESET_BYTES,
  NAME_BYTES,
  NUM_PARAMS,
  MFG_ID,
  CMD_PUT_PRESET,
  RESP_PRESET_DATA,
  RESP_LIVE_STATE,
  RESP_ACK,
  CC_MOD_BASE,
  CC_DELAY_BASE,
  CC_REVERB_BASE,
} from "../lib/sysex";

// ── helpers ───────────────────────────────────────────────────────────────────

function makePresetDataFrame(bank: number, slot: number, name: string, raw: Uint8Array): Uint8Array {
  const nameBytes = new Uint8Array(NAME_BYTES);
  for (let i = 0; i < Math.min(name.length, NAME_BYTES - 1); i++) {
    nameBytes[i] = name.charCodeAt(i);
  }
  const encoded = encode7bit(raw);
  const frame = new Uint8Array(1 + 1 + 1 + 1 + 1 + NAME_BYTES + encoded.length + 1);
  let off = 0;
  frame[off++] = 0xF0;
  frame[off++] = MFG_ID;
  frame[off++] = RESP_PRESET_DATA;
  frame[off++] = bank;
  frame[off++] = slot;
  frame.set(nameBytes, off); off += NAME_BYTES;
  frame.set(encoded, off);  off += encoded.length;
  frame[off]   = 0xF7;
  return frame;
}

function makeLiveStateFrame(bank: number, slot: number, raw: Uint8Array): Uint8Array {
  const encoded = encode7bit(raw);
  const frame = new Uint8Array(1 + 1 + 1 + 1 + 1 + encoded.length + 1);
  let off = 0;
  frame[off++] = 0xF0;
  frame[off++] = MFG_ID;
  frame[off++] = RESP_LIVE_STATE;
  frame[off++] = bank;
  frame[off++] = slot;
  frame.set(encoded, off); off += encoded.length;
  frame[off]   = 0xF7;
  return frame;
}

function makeAckFrame(originalCmd: number, ok: boolean): Uint8Array {
  return new Uint8Array([0xF0, MFG_ID, RESP_ACK, originalCmd, ok ? 0x00 : 0x01, 0xF7]);
}

// ── 7-bit codec ───────────────────────────────────────────────────────────────

describe("encode7bit / decode7bit", () => {
  it("encodes 92 bytes to exactly 106 bytes (firmware invariant)", () => {
    const input = new Uint8Array(RAW_PRESET_BYTES).fill(0xAA);
    const encoded = encode7bit(input);
    expect(encoded.length).toBe(ENCODED_PRESET_BYTES);
  });

  it("all output bytes are < 0x80 (SysEx safe)", () => {
    const input = new Uint8Array(RAW_PRESET_BYTES).fill(0xFF);
    const encoded = encode7bit(input);
    for (const b of encoded) {
      expect(b).toBeLessThan(0x80);
    }
  });

  it("round-trips sequential bytes 0x00..0x5B (92 bytes)", () => {
    const input = new Uint8Array(RAW_PRESET_BYTES).map((_, i) => i);
    expect(decode7bit(encode7bit(input))).toEqual(input);
  });

  it("round-trips all-zero payload", () => {
    const input = new Uint8Array(RAW_PRESET_BYTES);
    expect(decode7bit(encode7bit(input))).toEqual(input);
  });

  it("round-trips all-0xFF payload (all high bits set)", () => {
    const input = new Uint8Array(RAW_PRESET_BYTES).fill(0xFF);
    expect(decode7bit(encode7bit(input))).toEqual(input);
  });

  it("encode is deterministic (matches Rust algorithm byte-for-byte)", () => {
    // Hand-computed: input = [0x80, 0x01] → chunk=[0x80,0x01], msb=(0x80>>7)=1 → msb=0x01
    // output = [0x01, 0x00, 0x01]
    const input = new Uint8Array([0x80, 0x01]);
    const encoded = encode7bit(input);
    expect(Array.from(encoded)).toEqual([0x01, 0x00, 0x01]);
  });

  it("handles empty input without throwing", () => {
    expect(encode7bit(new Uint8Array(0)).length).toBe(0);
    expect(decode7bit(new Uint8Array(0)).length).toBe(0);
  });

  it("handles single byte round-trip", () => {
    for (const v of [0x00, 0x7F, 0x80, 0xFF]) {
      const input = new Uint8Array([v]);
      expect(decode7bit(encode7bit(input))).toEqual(input);
    }
  });
});

// ── ACK frame parser ──────────────────────────────────────────────────────────

describe("parseAckFrame", () => {
  it("parses a successful PUT_PRESET ACK", () => {
    const frame = makeAckFrame(CMD_PUT_PRESET, true);
    const result = parseAckFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.originalCmd).toBe(CMD_PUT_PRESET);
    expect(result!.ok).toBe(true);
  });

  it("parses a failed PUT_PRESET ACK", () => {
    const frame = makeAckFrame(CMD_PUT_PRESET, false);
    const result = parseAckFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.ok).toBe(false);
  });

  it("ok=true ONLY when byte[4] === 0x00 (firmware convention)", () => {
    const success = new Uint8Array([0xF0, MFG_ID, RESP_ACK, CMD_PUT_PRESET, 0x00, 0xF7]);
    const failure = new Uint8Array([0xF0, MFG_ID, RESP_ACK, CMD_PUT_PRESET, 0x01, 0xF7]);
    expect(parseAckFrame(success)!.ok).toBe(true);
    expect(parseAckFrame(failure)!.ok).toBe(false);
  });

  it("returns null for wrong manufacturer ID", () => {
    const frame = new Uint8Array([0xF0, 0x41, RESP_ACK, CMD_PUT_PRESET, 0x00, 0xF7]);
    expect(parseAckFrame(frame)).toBeNull();
  });

  it("returns null for wrong command byte", () => {
    const frame = new Uint8Array([0xF0, MFG_ID, RESP_PRESET_DATA, CMD_PUT_PRESET, 0x00, 0xF7]);
    expect(parseAckFrame(frame)).toBeNull();
  });

  it("returns null for frames shorter than 6 bytes", () => {
    expect(parseAckFrame(new Uint8Array([0xF0, MFG_ID, RESP_ACK, CMD_PUT_PRESET]))).toBeNull();
    expect(parseAckFrame(new Uint8Array([]))).toBeNull();
  });

  it("does not crash on non-SysEx bytes", () => {
    expect(parseAckFrame(new Uint8Array([0xB0, 0x0E, 0x40]))).toBeNull();
  });

  it("returns null when F7 terminator is missing (truncated frame)", () => {
    // Frame without F7 at the end — simulates a truncated/dropped byte.
    const frame = new Uint8Array([0xF0, MFG_ID, RESP_ACK, CMD_PUT_PRESET, 0x00, 0x00]);
    expect(parseAckFrame(frame)).toBeNull();
  });
});

// ── PRESET_DATA frame parser ──────────────────────────────────────────────────

describe("parsePresetDataFrame", () => {
  it("extracts bank, slot, name, and raw payload", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES).map((_, i) => i % 256);
    const frame = makePresetDataFrame(2, 7, "MyPreset", raw);
    const result = parsePresetDataFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.bank).toBe(2);
    expect(result!.slot).toBe(7);
    expect(result!.name).toBe("MyPreset");
    expect(result!.rawData).toEqual(raw);
  });

  it("strips trailing null bytes from name", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makePresetDataFrame(0, 0, "Hi", raw);
    expect(parsePresetDataFrame(frame)!.name).toBe("Hi");
  });

  it("decodes raw payload to exactly RAW_PRESET_BYTES", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES).fill(0x55);
    const frame = makePresetDataFrame(0, 0, "Test", raw);
    expect(parsePresetDataFrame(frame)!.rawData.length).toBe(RAW_PRESET_BYTES);
  });

  it("returns null for wrong command byte", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makeLiveStateFrame(0, 0, raw); // cmd 0x42 ≠ 0x41
    expect(parsePresetDataFrame(frame)).toBeNull();
  });

  it("returns null for frame too short to contain name", () => {
    const frame = new Uint8Array([0xF0, MFG_ID, RESP_PRESET_DATA, 0, 0, 0xF7]);
    expect(parsePresetDataFrame(frame)).toBeNull();
  });

  it("returns null when F7 terminator is missing (truncated frame)", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makePresetDataFrame(0, 0, "Test", raw);
    // Replace F7 with a non-terminator byte to simulate truncation.
    const truncated = new Uint8Array(frame);
    truncated[truncated.length - 1] = 0x00;
    expect(parsePresetDataFrame(truncated)).toBeNull();
  });

  it("round-trips 92-byte payload faithfully (codec integration)", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES).map((_, i) => (i * 17 + 3) % 256);
    const frame = makePresetDataFrame(5, 3, "RoundTrip", raw);
    const parsed = parsePresetDataFrame(frame)!;
    for (let i = 0; i < RAW_PRESET_BYTES; i++) {
      expect(parsed.rawData[i]).toBe(raw[i]);
    }
  });
});

// ── LIVE_STATE frame parser ───────────────────────────────────────────────────

describe("parseLiveStateFrame", () => {
  it("extracts bank, slot, and raw payload", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES).fill(0xAA);
    const frame = makeLiveStateFrame(3, 9, raw);
    const result = parseLiveStateFrame(frame);
    expect(result).not.toBeNull();
    expect(result!.bank).toBe(3);
    expect(result!.slot).toBe(9);
    expect(result!.rawData).toEqual(raw);
  });

  it("decodes raw payload to exactly RAW_PRESET_BYTES", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makeLiveStateFrame(0, 0, raw);
    expect(parseLiveStateFrame(frame)!.rawData.length).toBe(RAW_PRESET_BYTES);
  });

  it("returns null for wrong command byte", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makePresetDataFrame(0, 0, "X", raw); // cmd 0x41 ≠ 0x42
    expect(parseLiveStateFrame(frame)).toBeNull();
  });

  it("returns null for frame shorter than 6 bytes", () => {
    expect(parseLiveStateFrame(new Uint8Array([0xF0, MFG_ID, RESP_LIVE_STATE, 0, 0xF7]))).toBeNull();
    expect(parseLiveStateFrame(new Uint8Array([]))).toBeNull();
  });

  it("returns null when F7 terminator is missing (truncated frame)", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const frame = makeLiveStateFrame(0, 0, raw);
    const truncated = new Uint8Array(frame);
    truncated[truncated.length - 1] = 0x00;
    expect(parseLiveStateFrame(truncated)).toBeNull();
  });

  it("round-trips a real-world preset payload with floats", () => {
    const raw = new Uint8Array(RAW_PRESET_BYTES);
    const view = new DataView(raw.buffer);
    raw[1] = 2;  // modMode = Chorus
    raw[2] = 4;  // delayMode = PingPong
    view.setFloat32(4,  0.75, true); // mod param 0
    view.setFloat32(32, 0.5,  true); // delay param 0
    raw[88] = 1; raw[89] = 1; raw[90] = 0;
    const frame  = makeLiveStateFrame(1, 2, raw);
    const parsed = parseLiveStateFrame(frame)!;
    expect(parsed.rawData[1]).toBe(2);
    expect(parsed.rawData[2]).toBe(4);
    expect(parsed.rawData[88]).toBe(1);
    expect(new DataView(parsed.rawData.buffer).getFloat32(4, true)).toBeCloseTo(0.75, 5);
  });
});

// ── CC helpers ────────────────────────────────────────────────────────────────

describe("normalisedToCC", () => {
  it("maps 0.0 → 0",   () => expect(normalisedToCC(0.0)).toBe(0));
  it("maps 1.0 → 127", () => expect(normalisedToCC(1.0)).toBe(127));
  it("maps 0.5 → 64",  () => expect(normalisedToCC(0.5)).toBe(64));
  it("clamps values below 0", () => expect(normalisedToCC(-1)).toBe(0));
  it("clamps values above 1", () => expect(normalisedToCC(2)).toBe(127));
  it("rounds to nearest integer", () => {
    expect(normalisedToCC(0.504)).toBe(64);
    expect(normalisedToCC(0.496)).toBe(63);
  });
});

describe("ccNumber", () => {
  it("mod stage: CC_MOD_BASE + paramIndex", () => {
    for (let i = 0; i < 7; i++) {
      expect(ccNumber("mod", i)).toBe(CC_MOD_BASE + i);
    }
  });

  it("delay stage: CC_DELAY_BASE + paramIndex", () => {
    for (let i = 0; i < 7; i++) {
      expect(ccNumber("delay", i)).toBe(CC_DELAY_BASE + i);
    }
  });

  it("reverb stage: CC_REVERB_BASE + paramIndex", () => {
    for (let i = 0; i < 7; i++) {
      expect(ccNumber("reverb", i)).toBe(CC_REVERB_BASE + i);
    }
  });

  it("CC ranges do not overlap (no stage ambiguity)", () => {
    const seen = new Set<number>();
    for (const stage of ["mod", "delay", "reverb"] as const) {
      for (let i = 0; i < 7; i++) {
        const cc = ccNumber(stage, i);
        expect(seen.has(cc)).toBe(false);
        seen.add(cc);
      }
    }
  });
});

// ── ccToMapping ───────────────────────────────────────────────────────────────

describe("ccToMapping", () => {
  it("maps each mod CC to stage=mod with correct paramIndex", () => {
    for (let i = 0; i < NUM_PARAMS; i++) {
      const m = ccToMapping(CC_MOD_BASE + i);
      expect(m).not.toBeNull();
      expect(m!.stage).toBe("mod");
      expect(m!.paramIndex).toBe(i);
    }
  });

  it("maps each delay CC to stage=delay with correct paramIndex", () => {
    for (let i = 0; i < NUM_PARAMS; i++) {
      const m = ccToMapping(CC_DELAY_BASE + i);
      expect(m).not.toBeNull();
      expect(m!.stage).toBe("delay");
      expect(m!.paramIndex).toBe(i);
    }
  });

  it("maps each reverb CC to stage=reverb with correct paramIndex", () => {
    for (let i = 0; i < NUM_PARAMS; i++) {
      const m = ccToMapping(CC_REVERB_BASE + i);
      expect(m).not.toBeNull();
      expect(m!.stage).toBe("reverb");
      expect(m!.paramIndex).toBe(i);
    }
  });

  it("returns null for CCs outside all three mapped ranges", () => {
    // Mapped: 14-20 (mod), 21-27 (delay), 28-34 (reverb). Everything else is unmapped.
    const unmapped = [0, 1, 13, CC_REVERB_BASE + NUM_PARAMS, 64, 65, 100, 127];
    for (const cc of unmapped) {
      expect(ccToMapping(cc)).toBeNull();
    }
  });

  it("is the inverse of ccNumber for all valid (stage, index) pairs", () => {
    for (const stage of ["mod", "delay", "reverb"] as const) {
      for (let i = 0; i < NUM_PARAMS; i++) {
        const cc = ccNumber(stage, i);
        const m  = ccToMapping(cc);
        expect(m).not.toBeNull();
        expect(m!.stage).toBe(stage);
        expect(m!.paramIndex).toBe(i);
      }
    }
  });

  it("no CC maps to two different stages (ranges are disjoint)", () => {
    const seen = new Map<number, string>();
    for (const stage of ["mod", "delay", "reverb"] as const) {
      for (let i = 0; i < NUM_PARAMS; i++) {
        const cc = ccNumber(stage, i);
        expect(seen.has(cc)).toBe(false);
        seen.set(cc, stage);
      }
    }
  });
});

// ── Protocol constants (sanity-check against firmware) ────────────────────────

describe("protocol constants", () => {
  it("RAW_PRESET_BYTES is 92 (sizeof MultiPresetSlot)", () => {
    expect(RAW_PRESET_BYTES).toBe(92);
  });

  it("ENCODED_PRESET_BYTES is 106 (92-byte 7-bit encoding: 13*8+2)", () => {
    expect(ENCODED_PRESET_BYTES).toBe(106);
    expect(encode7bit(new Uint8Array(RAW_PRESET_BYTES)).length).toBe(ENCODED_PRESET_BYTES);
  });

  it("MFG_ID is 0x7D", () => expect(MFG_ID).toBe(0x7D));

  it("CC bases match firmware constants.h", () => {
    expect(CC_MOD_BASE).toBe(14);
    expect(CC_DELAY_BASE).toBe(21);
    expect(CC_REVERB_BASE).toBe(28);
  });

  it("NUM_PARAMS matches the spacing between CC base values", () => {
    expect(CC_DELAY_BASE  - CC_MOD_BASE).toBe(NUM_PARAMS);
    expect(CC_REVERB_BASE - CC_DELAY_BASE).toBe(NUM_PARAMS);
  });

  it("response command bytes are 7-bit (SysEx payload must be < 0x80)", () => {
    // A byte with bit 7 set inside a SysEx payload is read as a MIDI status
    // byte by the USB-MIDI packetizer, which corrupts the whole frame. These
    // were once 0x81/0x82/0x83, which silently broke all device→host messages.
    expect(RESP_PRESET_DATA & 0x80).toBe(0);
    expect(RESP_LIVE_STATE  & 0x80).toBe(0);
    expect(RESP_ACK         & 0x80).toBe(0);
  });
});
