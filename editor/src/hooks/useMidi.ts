import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState, useCallback } from "react";
import { parsePresetSlot, ParsedPreset } from "../lib/presetCodec";
import {
  CC_MOD_BASE, CC_DELAY_BASE, CC_REVERB_BASE,
  RESP_PRESET_DATA, RESP_LIVE_STATE, RESP_ACK,
  parsePresetDataFrame, parseLiveStateFrame, parseAckFrame,
  ccToMapping,
} from "../lib/sysex";

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

export interface UseMidiOptions {
  /** Called whenever the hardware sends a CC that maps to a known parameter.
   *  Normalised value is in [0, 1]. Do NOT send CC back to the device inside
   *  this callback — that would create a feedback loop. */
  onCC?: (stage: "mod" | "delay" | "reverb", paramIndex: number, normalised: number) => void;
}

export function useMidi(options: UseMidiOptions = {}) {
  const [ports, setPorts] = useState<string[]>([]);
  const [connected, setConnected] = useState(false);
  const [inputConnected, setInputConnected] = useState<boolean | null>(null);
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
  const savePending     = useRef<SavePending[]>([]);
  const connectedPort   = useRef<string | null>(null);
  const lastPort        = useRef<string | null>(null); // remembered for Reconnect
  const justConnected   = useRef(false);
  // Always points to the latest onCC without needing to re-register event listeners.
  const onCCRef         = useRef(options.onCC);
  onCCRef.current       = options.onCC;

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
    lastPort.current      = portName;
    setConnected(true);
    // get_live_state is sent from the midi-input-status listener once the input
    // thread confirms it is live — avoids a race where the response arrives before
    // the callback is registered.
    justConnected.current = true;
  }, []);

  const disconnect = useCallback(async () => {
    await invoke("disconnect_midi").catch(() => {});
    // Clear connectedPort so the ports-changed watcher does NOT auto-reconnect a
    // port the user deliberately dropped. lastPort still drives the Reconnect button.
    connectedPort.current = null;
    setConnected(false);
    setInputConnected(null);
  }, []);

  const reconnect = useCallback(async () => {
    const port = lastPort.current ?? connectedPort.current;
    if (!port) return;
    // connect() tears down any prior connection on the Rust side first.
    try { await connect(port); }
    catch (e) { reportError(`Reconnect failed: ${e}`); }
  }, [connect, reportError]);

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
      return new Promise<boolean>((resolve) => {
        const timer = setTimeout(() => {
          // Remove this entry from the queue on timeout (it may not be at the front
          // if an earlier save is still pending, but that is an exceptional case).
          const idx = savePending.current.findIndex((p) => p.resolve === resolve);
          if (idx !== -1) savePending.current.splice(idx, 1);
          resolve(false);
        }, 2000);
        savePending.current.push({ resolve, timer, bank, slot, name, rawData });
        invoke("put_preset", {
          bank, slot, name, rawData: Array.from(rawData),
        }).catch(() => {
          const idx = savePending.current.findIndex((p) => p.resolve === resolve);
          if (idx !== -1) {
            clearTimeout(savePending.current[idx].timer);
            savePending.current.splice(idx, 1);
          }
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
      // Fast reject: must be a SysEx frame with our manufacturer ID and a valid F7 terminator.
      if (msg.length < 4 || msg[0] !== 0xF0 || msg[1] !== 0x7D || msg[msg.length - 1] !== 0xF7) return;
      const cmd = msg[2];

      if (cmd === RESP_PRESET_DATA) {
        const frame = parsePresetDataFrame(msg);
        if (!frame) return;
        const { bank, slot, name, rawData } = frame;
        const idx = bank * 10 + slot;
        presetsRef.current[idx] = { bank, slot, name, rawData };
        setPresets((prev) => {
          const next = [...prev];
          next[idx]  = { bank, slot, name, rawData };
          return next;
        });
        const pending = loadPending.current.get(`${bank}-${slot}`);
        if (pending) {
          clearTimeout(pending.timer);
          loadPending.current.delete(`${bank}-${slot}`);
          pending.resolve(toResult(parsePresetSlot(rawData), name));
        }
      } else if (cmd === RESP_LIVE_STATE) {
        const frame = parseLiveStateFrame(msg);
        if (!frame) return;
        const parsed = parsePresetSlot(frame.rawData);
        setLiveState({ bank: frame.bank, slot: frame.slot, ...parsed });
      } else if (cmd === RESP_ACK) {
        const ack = parseAckFrame(msg);
        if (!ack) return;
        if (ack.originalCmd === 0x02 && savePending.current.length > 0) {
          // ACKs arrive in FIFO order — consume from the front of the queue.
          const pending = savePending.current.shift()!;
          clearTimeout(pending.timer);
          const { resolve, bank, slot, name, rawData } = pending;
          if (ack.ok) {
            const updated: PresetData = { bank, slot, name, rawData };
            presetsRef.current[bank * 10 + slot] = updated;
            setPresets((prev) => {
              const next = [...prev];
              next[bank * 10 + slot] = updated;
              return next;
            });
          }
          resolve(ack.ok);
        }
      }
    });
    return () => { unlisten.then((fn) => fn()); };
  }, []);

  useEffect(() => {
    const u1 = listen<number>("midi-sync-progress", (e) => setSyncProgress(e.payload));
    const u2 = listen<void>("midi-sync-done",       () => setSyncProgress(null));
    const u3 = listen<boolean>("midi-input-status", (e) => {
      setInputConnected(e.payload);
      // Input thread just confirmed it is live — safe to request live state now.
      if (e.payload && justConnected.current) {
        justConnected.current = false;
        invoke("get_live_state").catch(() => {});
      }
    });
    // Hardware knob → UI: CC messages from the device update the parameter sliders.
    const u4 = listen<number[]>("midi-cc", (e) => {
      const [cc, value] = e.payload;
      const mapping = ccToMapping(cc);
      if (!mapping) return;
      const normalised = value / 127;
      onCCRef.current?.(mapping.stage, mapping.paramIndex, normalised);
    });
    return () => { u1.then((f) => f()); u2.then((f) => f()); u3.then((f) => f()); u4.then((f) => f()); };
  }, []);

  // Auto-update port list; auto-reconnect if connected port reappears (e.g. after firmware flash).
  useEffect(() => {
    const unlisten = listen<string[]>("midi-ports-changed", (event) => {
      const newPorts = event.payload;
      setPorts(newPorts);
      const port = connectedPort.current;
      if (!port) return;
      if (newPorts.includes(port)) {
        // Port reappeared — re-establish connection; live state is fetched via
        // the midi-input-status event once input is confirmed live.
        invoke("connect_midi", { portName: port })
          .then(() => {
            setConnected(true);
            justConnected.current = true;
          })
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
    inputConnected,
    presets,
    setPresets,
    midiError,
    liveState,
    syncProgress,
    refreshPorts,
    connect,
    disconnect,
    reconnect,
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

