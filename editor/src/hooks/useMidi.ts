import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState, useCallback } from "react";

// CC base values matching firmware constants.h
const CC_MOD_BASE    = 14;
const CC_DELAY_BASE  = 21;
const CC_REVERB_BASE = 28;

export interface PresetData {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array; // 92 bytes
}

export function useMidi() {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [presets, setPresets] = useState<(PresetData | null)[]>(
    Array(100).fill(null)
  );
  const ccThrottle = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());

  const refreshPorts = useCallback(async () => {
    const p = await invoke<string[]>("list_midi_ports");
    setPorts(p);
  }, []);

  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    setConnected(true);
  }, []);

  // Send CC with 10ms throttle per knob.
  const sendCC = useCallback(
    (stage: "mod" | "delay" | "reverb", paramIndex: number, normalised: number) => {
      const base =
        stage === "mod" ? CC_MOD_BASE :
        stage === "delay" ? CC_DELAY_BASE : CC_REVERB_BASE;
      const cc    = base + paramIndex;
      const value = Math.round(normalised * 127);
      const key   = `${cc}`;

      if (ccThrottle.current.has(key)) clearTimeout(ccThrottle.current.get(key)!);
      ccThrottle.current.set(
        key,
        setTimeout(() => {
          invoke("send_cc", { channel: 0, cc, value }).catch(console.error);
          ccThrottle.current.delete(key);
        }, 10)
      );
    },
    []
  );

  const setActivePreset = useCallback((bank: number, slot: number) => {
    invoke("set_active_preset", { bank, slot }).catch(console.error);
  }, []);

  const setMode = useCallback((stage: number, modeIndex: number) => {
    invoke("set_mode", { stage, modeIndex }).catch(console.error);
  }, []);

  const getAllPresets = useCallback(() => {
    invoke("get_all_presets").catch(console.error);
  }, []);

  const putPreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array) => {
      invoke("put_preset", {
        bank,
        slot,
        name,
        rawData: Array.from(rawData),
      }).catch(console.error);
    },
    []
  );

  // Listen for incoming SysEx from the device.
  useEffect(() => {
    const unlisten = listen<number[]>("midi-sysex", (event) => {
      const msg = new Uint8Array(event.payload);
      // PRESET_DATA response: F0 7D 81 bank slot name[12] encoded[106] F7
      if (msg.length >= 5 && msg[0] === 0xf0 && msg[1] === 0x7d && msg[2] === 0x81) {
        const bank = msg[3];
        const slot = msg[4];
        const nameBytes = msg.slice(5, 17);
        const name = new TextDecoder()
          .decode(nameBytes)
          .replace(/\0+$/, "");
        const encoded = msg.slice(17, msg.length - 1);
        const rawData = decode7bit(encoded);
        const idx = bank * 10 + slot;
        setPresets((prev) => {
          const next = [...prev];
          next[idx] = { bank, slot, name, rawData };
          return next;
        });
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, []);

  return {
    ports,
    connected,
    presets,
    refreshPorts,
    connect,
    sendCC,
    setActivePreset,
    setMode,
    getAllPresets,
    putPreset,
  };
}

// Mirror of the Rust decode_7bit (runs in the browser for incoming SysEx).
function decode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  let i = 0;
  while (i < input.length) {
    const remaining = input.length - i;
    if (remaining < 2) break;
    const chunkLen = Math.min(remaining - 1, 7);
    const msb = input[i++];
    for (let j = 0; j < chunkLen; j++) {
      out.push(input[i++] | (((msb >> j) & 1) << 7));
    }
  }
  return new Uint8Array(out);
}
