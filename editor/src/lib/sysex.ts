// SysEx protocol constants and frame helpers.
// These must stay in sync with the C++ firmware (src/midi/sysex_codec.cpp)
// and the Rust backend (src-tauri/src/sysex.rs).

export const MFG_ID = 0x7D;

// Host-to-device commands
export const CMD_GET_PRESET  = 0x01;
export const CMD_PUT_PRESET  = 0x02;
export const CMD_SET_ACTIVE  = 0x04;
export const CMD_GET_ALL     = 0x05;
export const CMD_SET_MODE    = 0x07;
export const CMD_SET_FX      = 0x08;
export const CMD_GET_LIVE    = 0x0B;

// Device-to-host responses.
// MUST be < 0x80: these bytes travel inside the SysEx payload, and any byte
// with bit 7 set is interpreted as a MIDI status byte by the USB-MIDI packetizer
// (corrupting the frame). Originally 0x81/0x82/0x83 — which silently broke all
// device→host messages on macOS.
export const RESP_PRESET_DATA = 0x41;
export const RESP_LIVE_STATE  = 0x42;
export const RESP_ACK         = 0x43;

export const CC_MOD_BASE    = 14;
export const CC_DELAY_BASE  = 21;
export const CC_REVERB_BASE = 28;
export const NUM_PARAMS     = 7; // params per stage; must match firmware constants.h

export const RAW_PRESET_BYTES     = 92;
export const ENCODED_PRESET_BYTES = 106; // (92/7)*8 + (92%7 + 1)
export const NAME_BYTES           = 12;
export const NAME_MAX_CHARS       = 11;

// ── 7-bit codec ───────────────────────────────────────────────────────────────
// Mirrors encode_7bit / decode_7bit in src-tauri/src/sysex.rs and
// sysex_codec.cpp in the firmware.  Every 7 input bytes → 8 output bytes.
// Invariant: all output bytes are < 0x80 (safe for MIDI SysEx).

export function encode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  for (let i = 0; i < input.length; i += 7) {
    const chunk = input.subarray(i, Math.min(i + 7, input.length));
    let msb = 0;
    for (let j = 0; j < chunk.length; j++) {
      msb |= ((chunk[j] >> 7) & 1) << j;
    }
    out.push(msb);
    for (let j = 0; j < chunk.length; j++) {
      out.push(chunk[j] & 0x7F);
    }
  }
  return new Uint8Array(out);
}

export function decode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  let i = 0;
  while (i < input.length) {
    const remaining = input.length - i;
    if (remaining < 2) break;
    const chunkLen = Math.min(remaining - 1, 7);
    const msb      = input[i++];
    for (let j = 0; j < chunkLen; j++) {
      out.push(input[i++] | (((msb >> j) & 1) << 7));
    }
  }
  return new Uint8Array(out);
}

// ── Frame parsers (device → host) ────────────────────────────────────────────

export interface AckFrame {
  originalCmd: number;
  ok: boolean;
}

/** Parse ACK (cmd 0x43): F0 7D 43 originalCmd ok [activeBank activeSlot] F7 */
export function parseAckFrame(msg: Uint8Array): AckFrame | null {
  if (msg.length < 6 || msg[0] !== 0xF0 || msg[1] !== MFG_ID || msg[2] !== RESP_ACK || msg[msg.length - 1] !== 0xF7) {
    return null;
  }
  return { originalCmd: msg[3], ok: msg[4] === 0x00 };
}

export interface PresetDataFrame {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array;
}

/** Parse PRESET_DATA (cmd 0x41): F0 7D 41 bank slot name[12] encoded[106] F7 */
export function parsePresetDataFrame(msg: Uint8Array): PresetDataFrame | null {
  if (msg.length < 5 + NAME_BYTES || msg[0] !== 0xF0 || msg[1] !== MFG_ID || msg[2] !== RESP_PRESET_DATA || msg[msg.length - 1] !== 0xF7) {
    return null;
  }
  const bank      = msg[3];
  const slot      = msg[4];
  const nameBytes = msg.slice(5, 5 + NAME_BYTES);
  const name      = new TextDecoder().decode(nameBytes).replace(/\0+$/, "");
  const encoded   = msg.slice(5 + NAME_BYTES, msg.length - 1); // strip F7
  const rawData   = decode7bit(encoded);
  return { bank, slot, name, rawData };
}

export interface LiveStateFrame {
  bank: number;
  slot: number;
  rawData: Uint8Array;
}

/** Parse LIVE_STATE (cmd 0x42): F0 7D 42 bank slot encoded[106] F7 */
export function parseLiveStateFrame(msg: Uint8Array): LiveStateFrame | null {
  if (msg.length < 6 || msg[0] !== 0xF0 || msg[1] !== MFG_ID || msg[2] !== RESP_LIVE_STATE || msg[msg.length - 1] !== 0xF7) {
    return null;
  }
  const bank    = msg[3];
  const slot    = msg[4];
  const rawData = decode7bit(msg.slice(5, msg.length - 1)); // strip F7
  return { bank, slot, rawData };
}

// ── CC helpers ────────────────────────────────────────────────────────────────

/** Map a normalised [0, 1] value to a 7-bit MIDI CC value [0, 127]. */
export function normalisedToCC(normalised: number): number {
  return Math.round(Math.max(0, Math.min(1, normalised)) * 127);
}

/** Return the CC number for a given stage and parameter index. */
export function ccNumber(stage: "mod" | "delay" | "reverb", paramIndex: number): number {
  const base = stage === "mod" ? CC_MOD_BASE : stage === "delay" ? CC_DELAY_BASE : CC_REVERB_BASE;
  return base + paramIndex;
}

export interface CCMapping {
  stage: "mod" | "delay" | "reverb";
  paramIndex: number;
}

/** Reverse-map a CC number back to its stage and parameter index.
 *  Returns null if the CC is not in any of the three mapped ranges. */
export function ccToMapping(cc: number): CCMapping | null {
  if (cc >= CC_MOD_BASE    && cc < CC_MOD_BASE    + NUM_PARAMS) return { stage: "mod",    paramIndex: cc - CC_MOD_BASE };
  if (cc >= CC_DELAY_BASE  && cc < CC_DELAY_BASE  + NUM_PARAMS) return { stage: "delay",  paramIndex: cc - CC_DELAY_BASE };
  if (cc >= CC_REVERB_BASE && cc < CC_REVERB_BASE + NUM_PARAMS) return { stage: "reverb", paramIndex: cc - CC_REVERB_BASE };
  return null;
}
