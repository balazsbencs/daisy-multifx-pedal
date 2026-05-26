import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useCallback, useEffect, useRef, useState } from "react";

const CC_MOD_BASE = 14;
const CC_DELAY_BASE = 21;
const CC_REVERB_BASE = 28;

export interface PresetData {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array;
}

type Stage = "mod" | "delay" | "reverb";

export function useMidi() {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [presets, setPresets] = useState<(PresetData | null)[]>(() => Array(100).fill(null));
  const lastSentAt = useRef<Map<number, number>>(new Map());
  const pending = useRef<Map<number, ReturnType<typeof setTimeout>>>(new Map());

  const refreshPorts = useCallback(async () => {
    setPorts(await invoke<string[]>("list_midi_ports"));
  }, []);

  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    setConnected(true);
  }, []);

  const sendCC = useCallback((stage: Stage, paramIndex: number, normalised: number) => {
    const base = stage === "mod" ? CC_MOD_BASE : stage === "delay" ? CC_DELAY_BASE : CC_REVERB_BASE;
    const cc = base + paramIndex;
    const value = Math.round(Math.max(0, Math.min(1, normalised)) * 127);
    const now = performance.now();
    const last = lastSentAt.current.get(cc) ?? 0;

    const send = () => {
      lastSentAt.current.set(cc, performance.now());
      pending.current.delete(cc);
      void invoke("send_cc", { channel: 0, cc, value }).catch(console.error);
    };

    if (now - last >= 10) {
      if (pending.current.has(cc)) {
        clearTimeout(pending.current.get(cc));
        pending.current.delete(cc);
      }
      send();
      return;
    }

    if (pending.current.has(cc)) {
      clearTimeout(pending.current.get(cc));
    }
    pending.current.set(cc, setTimeout(send, 10 - (now - last)));
  }, []);

  const setActivePreset = useCallback((bank: number, slot: number) => {
    return invoke("set_active_preset", { bank, slot }).catch(console.error);
  }, []);

  const setMode = useCallback((stage: number, modeIndex: number) => {
    return invoke("set_mode", { stage, modeIndex }).catch(console.error);
  }, []);

  const getPreset = useCallback((bank: number, slot: number) => {
    return invoke("get_preset", { bank, slot }).catch(console.error);
  }, []);

  const getAllPresets = useCallback(() => {
    return invoke("get_all_presets").catch(console.error);
  }, []);

  const putPreset = useCallback((bank: number, slot: number, name: string, rawData: Uint8Array) => {
    return invoke("put_preset", {
      bank,
      slot,
      name,
      rawData: Array.from(rawData),
    }).catch(console.error);
  }, []);

  useEffect(() => {
    const unlisten = listen<number[]>("midi-sysex", (event) => {
      const msg = new Uint8Array(event.payload);
      if (msg.length < 18 || msg[0] !== 0xf0 || msg[1] !== 0x7d || msg[2] !== 0x81) {
        return;
      }

      const bank = msg[3];
      const slot = msg[4];
      const name = new TextDecoder().decode(msg.slice(5, 17)).replace(/\0+$/, "");
      const rawData = decode7bit(msg.slice(17, msg.length - 1));
      const idx = bank * 10 + slot;

      setPresets((prev) => {
        const next = [...prev];
        next[idx] = { bank, slot, name, rawData };
        return next;
      });
    });

    return () => {
      void unlisten.then((fn) => fn());
      for (const timeout of pending.current.values()) {
        clearTimeout(timeout);
      }
      pending.current.clear();
    };
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
    getPreset,
    getAllPresets,
    putPreset,
  };
}

function decode7bit(input: Uint8Array): Uint8Array {
  const out: number[] = [];
  let i = 0;

  while (i < input.length) {
    const remaining = input.length - i;
    if (remaining < 2) break;

    const chunkLen = Math.min(remaining - 1, 7);
    const msb = input[i++];
    for (let bit = 0; bit < chunkLen; bit++) {
      out.push(input[i++] | (((msb >> bit) & 1) << 7));
    }
  }

  return new Uint8Array(out);
}
