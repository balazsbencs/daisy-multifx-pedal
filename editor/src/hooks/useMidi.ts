import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState, useCallback } from "react";
import { parsePresetSlot, ParsedPreset } from "../lib/presetCodec";

const CC_MOD_BASE    = 14;
const CC_DELAY_BASE  = 21;
const CC_REVERB_BASE = 28;

export interface PresetData {
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array; // 92 bytes
}

export interface LoadedPresetResult {
  valid: number;
  modMode: number;
  delayMode: number;
  reverbMode: number;
  modParams: number[];
  delayParams: number[];
  reverbParams: number[];
  fxEnabled: [number, number, number];
  name: string;
}

export interface LiveStateUpdate {
  bank: number;
  slot: number;
  valid: number;
  modMode: number;
  delayMode: number;
  reverbMode: number;
  modParams: number[];
  delayParams: number[];
  reverbParams: number[];
  fxEnabled: [number, number, number];
}

interface LoadPending {
  resolve: (result: LoadedPresetResult | null) => void;
  timer: ReturnType<typeof setTimeout>;
}

interface SavePending {
  resolve: (ok: boolean) => void;
  timer: ReturnType<typeof setTimeout>;
  bank: number;
  slot: number;
  name: string;
  rawData: Uint8Array;
}

export function useMidi() {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [presets, setPresets] = useState<(PresetData | null)[]>(
    Array(100).fill(null)
  );
  const [midiError, setMidiError] = useState<string | null>(null);
  const [liveState, setLiveState] = useState<LiveStateUpdate | null>(null);
  const [syncProgress, setSyncProgress] = useState<number | null>(null);

  const ccThrottle      = useRef<Map<string, ReturnType<typeof setTimeout>>>(new Map());
  const errorTimer      = useRef<ReturnType<typeof setTimeout> | null>(null);
  const presetsRef      = useRef<(PresetData | null)[]>(Array(100).fill(null));
  const loadPending     = useRef<Map<string, LoadPending>>(new Map());
  const savePending     = useRef<SavePending | null>(null);
  const connectedPort   = useRef<string | null>(null);

  useEffect(() => { presetsRef.current = presets; }, [presets]);

  const reportError = useCallback((msg: string) => {
    setMidiError(msg);
    if (errorTimer.current) clearTimeout(errorTimer.current);
    errorTimer.current = setTimeout(() => setMidiError(null), 4000);
  }, []);

  const refreshPorts = useCallback(async () => {
    const p = await invoke<string[]>("list_midi_ports");
    setPorts(p);
  }, []);

  const connect = useCallback(async (portName: string) => {
    await invoke("connect_midi", { portName });
    connectedPort.current = portName;
    setConnected(true);
    // Send after connect_midi returns — by then the midir input thread is running
    // and the frontend midi-sysex listener (registered at mount) is ready.
    invoke("get_live_state").catch(() => {});
  }, []);

  const sendCC = useCallback(
    (stage: "mod" | "delay" | "reverb", paramIndex: number, normalised: number) => {
      const base =
        stage === "mod"   ? CC_MOD_BASE :
        stage === "delay" ? CC_DELAY_BASE : CC_REVERB_BASE;
      const cc    = base + paramIndex;
      const value = Math.round(normalised * 127);
      const key   = `${cc}`;
      if (ccThrottle.current.has(key)) clearTimeout(ccThrottle.current.get(key)!);
      ccThrottle.current.set(
        key,
        setTimeout(() => {
          invoke("send_cc", { channel: 0, cc, value }).catch((e) => {
            reportError(`CC send failed: ${e}`);
          });
          ccThrottle.current.delete(key);
        }, 10)
      );
    },
    [reportError]
  );

  const setMode = useCallback((stage: number, modeIndex: number) => {
    invoke("set_mode", { stage, modeIndex }).catch((e) => {
      reportError(`Mode change failed: ${e}`);
    });
  }, [reportError]);

  const setFxEnabled = useCallback((stage: number, enabled: boolean) => {
    invoke("set_fx_enabled", { stage, enabled }).catch((e) => {
      reportError(`FX toggle failed: ${e}`);
    });
  }, [reportError]);

  const syncAllPresets = useCallback(() => {
    invoke("sync_all_presets").catch((e) => {
      reportError(`Sync failed: ${e}`);
    });
  }, [reportError]);

  const putPreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array) => {
      invoke("put_preset", {
        bank, slot, name, rawData: Array.from(rawData),
      }).catch((e) => {
        reportError(`Put preset failed: ${e}`);
      });
    },
    [reportError]
  );

  const savePreset = useCallback(
    (bank: number, slot: number, name: string, rawData: Uint8Array): Promise<boolean> => {
      if (savePending.current) {
        clearTimeout(savePending.current.timer);
        savePending.current.resolve(false);
        savePending.current = null;
      }
      return new Promise<boolean>((resolve) => {
        const timer = setTimeout(() => {
          savePending.current = null;
          resolve(false);
        }, 2000);
        savePending.current = { resolve, timer, bank, slot, name, rawData };
        invoke("put_preset", {
          bank, slot, name, rawData: Array.from(rawData),
        }).catch(() => {
          clearTimeout(timer);
          savePending.current = null;
          resolve(false);
        });
      });
    },
    []
  );

  const loadPreset = useCallback(
    (bank: number, slot: number): Promise<LoadedPresetResult | null> => {
      invoke("set_active_preset", { bank, slot }).catch((e) => {
        reportError(`Set active failed: ${e}`);
      });
      const idx    = bank * 10 + slot;
      const cached = presetsRef.current[idx];
      if (cached) {
        return Promise.resolve(toResult(parsePresetSlot(cached.rawData), cached.name));
      }
      invoke("get_preset", { bank, slot }).catch((e) => {
        reportError(`Get preset failed: ${e}`);
      });
      return new Promise<LoadedPresetResult | null>((resolve) => {
        const key   = `${bank}-${slot}`;
        const timer = setTimeout(() => {
          loadPending.current.delete(key);
          resolve(null);
        }, 2000);
        loadPending.current.set(key, { resolve, timer });
      });
    },
    [reportError]
  );

  useEffect(() => {
    const unlisten = listen<number[]>("midi-sysex", (event) => {
      const msg = new Uint8Array(event.payload);
      if (msg.length < 3 || msg[0] !== 0xF0 || msg[1] !== 0x7D) return;
      const cmd = msg[2];

      if (cmd === 0x81) {
        if (msg.length < 5) return;
        const bank      = msg[3];
        const slot      = msg[4];
        const nameBytes = msg.slice(5, 17);
        const name      = new TextDecoder().decode(nameBytes).replace(/\0+$/, "");
        const encoded   = msg.slice(17, msg.length - 1);
        const rawData   = decode7bit(encoded);
        const idx       = bank * 10 + slot;
        presetsRef.current[idx] = { bank, slot, name, rawData };
        setPresets((prev) => {
          const next = [...prev];
          next[idx]  = { bank, slot, name, rawData };
          return next;
        });
        const key     = `${bank}-${slot}`;
        const pending = loadPending.current.get(key);
        if (pending) {
          clearTimeout(pending.timer);
          loadPending.current.delete(key);
          pending.resolve(toResult(parsePresetSlot(rawData), name));
        }
      } else if (cmd === 0x82) {
        if (msg.length < 7) return;
        const bank    = msg[3];
        const slot    = msg[4];
        const rawData = decode7bit(msg.slice(5, msg.length - 1));
        const parsed  = parsePresetSlot(rawData);
        setLiveState({ bank, slot, ...parsed });
      } else if (cmd === 0x83) {
        if (msg.length < 6) return;
        const originalCmd = msg[3];
        const ok          = msg[4] === 0x00;
        if (originalCmd === 0x02 && savePending.current) {
          clearTimeout(savePending.current.timer);
          const { resolve, bank, slot, name, rawData } = savePending.current;
          savePending.current = null;
          if (ok) {
            const updated: PresetData = { bank, slot, name, rawData };
            presetsRef.current[bank * 10 + slot] = updated;
            setPresets((prev) => {
              const next = [...prev];
              next[bank * 10 + slot] = updated;
              return next;
            });
          }
          resolve(ok);
        }
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, []);

  useEffect(() => {
    const u1 = listen<number>("midi-sync-progress", (e) => setSyncProgress(e.payload));
    const u2 = listen<void>("midi-sync-done",       () => setSyncProgress(null));
    return () => { u1.then((f) => f()); u2.then((f) => f()); };
  }, []);

  // Auto-update port list; auto-reconnect if connected port reappears (e.g. after firmware flash).
  useEffect(() => {
    const unlisten = listen<string[]>("midi-ports-changed", (event) => {
      const newPorts = event.payload;
      setPorts(newPorts);
      const port = connectedPort.current;
      if (!port) return;
      if (newPorts.includes(port)) {
        // Port reappeared — silently re-establish the output connection.
        invoke("connect_midi", { portName: port })
          .then(() => setConnected(true))
          .catch(() => {
            connectedPort.current = null;
            setConnected(false);
          });
      } else {
        // Port gone — mark disconnected so the user can reconnect manually.
        setConnected(false);
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, []);

  return {
    ports,
    connected,
    presets,
    setPresets,
    midiError,
    liveState,
    syncProgress,
    refreshPorts,
    connect,
    reportError,
    sendCC,
    setMode,
    setFxEnabled,
    syncAllPresets,
    putPreset,
    savePreset,
    loadPreset,
  };
}

function toResult(parsed: ParsedPreset, name: string): LoadedPresetResult {
  return { valid: parsed.valid, modMode: parsed.modMode, delayMode: parsed.delayMode,
           reverbMode: parsed.reverbMode, modParams: parsed.modParams,
           delayParams: parsed.delayParams, reverbParams: parsed.reverbParams,
           fxEnabled: parsed.fxEnabled, name };
}

function decode7bit(input: Uint8Array): Uint8Array {
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
