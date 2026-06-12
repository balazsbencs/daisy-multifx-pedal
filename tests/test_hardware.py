#!/usr/bin/env python3
"""
Hardware-in-loop test for the multi-fx Daisy pedal.

Requires: pip install mido python-rtmidi

Usage:
    python3 tests/test_hardware.py
    python3 tests/test_hardware.py --port "Daisy Seed"
    python3 tests/test_hardware.py --list

The device must be flashed and connected via USB.
Scratch slot: bank=9, slot=9 is overwritten then restored.
"""

import argparse
import struct
import sys
import time
from typing import Optional

try:
    import mido
except ImportError:
    print("ERROR: mido not installed.  Run:  pip install mido python-rtmidi")
    sys.exit(1)

# ── Protocol constants ──────────────────────────────────────────────────────
MFG_ID          = 0x7D
CMD_GET_PRESET  = 0x01
CMD_PUT_PRESET  = 0x02
CMD_SET_ACTIVE  = 0x04
CMD_GET_STATUS  = 0x06
CMD_SET_MODE    = 0x07
CMD_SET_FX      = 0x08
CMD_GET_LIVE    = 0x0B
RESP_PRESET     = 0x81  # preset data frame
RESP_LIVE       = 0x82  # live-state data frame
RESP_ACK        = 0x83  # ACK frame

CC_MOD_BASE    = 14
CC_DELAY_BASE  = 21
CC_REVERB_BASE = 28

# MultiPresetSlot (92 bytes, little-endian, ARM struct layout):
#   valid(1) mod_mode(1) delay_mode(1) reverb_mode(1)
#   mod_norm[7](28) delay_norm[7](28) reverb_norm[7](28)
#   fx_enabled[3](3) padding(1)
SLOT_FMT   = "<BBBB" + "f" * 21 + "BBBx"
SLOT_BYTES = struct.calcsize(SLOT_FMT)
assert SLOT_BYTES == 92, f"slot size {SLOT_BYTES} != 92"

# Encoded length: EncodedSize(92) = (92 // 7)*8 + (92 % 7) + 1 = 104 + 1 + 1 = 106
ENC_LEN = (SLOT_BYTES // 7) * 8 + (SLOT_BYTES % 7 + 1 if SLOT_BYTES % 7 else 0)

TIMEOUT_S     = 1.0   # max seconds to wait for a device response
CC_SETTLE_MS  = 80    # ms to wait after CC before requesting live state
SCRATCH_BANK  = 9
SCRATCH_SLOT  = 9

# ── 7-bit SysEx codec (matches sysex_codec.cpp exactly) ────────────────────

def encode7bit(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        chunk = data[i:i + 7]
        msb = 0
        for j, b in enumerate(chunk):
            if b & 0x80:
                msb |= (1 << j)
        out.append(msb)
        for b in chunk:
            out.append(b & 0x7F)
        i += 7
    return bytes(out)


def decode7bit(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        remaining = len(data) - i
        if remaining < 2:
            break
        chunk_len = min(remaining - 1, 7)
        msb = data[i]; i += 1
        for j in range(chunk_len):
            out.append(data[i] | (((msb >> j) & 1) << 7))
            i += 1
    return bytes(out)


# ── Slot pack / unpack ──────────────────────────────────────────────────────

def pack_slot(valid=1, mod_mode=0, delay_mode=0, reverb_mode=0,
              mod_norm=None, delay_norm=None, reverb_norm=None,
              fx_enabled=None) -> bytes:
    mod_norm    = list(mod_norm    or [0.0] * 7)
    delay_norm  = list(delay_norm  or [0.0] * 7)
    reverb_norm = list(reverb_norm or [0.0] * 7)
    fx_enabled  = list(fx_enabled  or [1, 1, 1])
    raw = struct.pack(SLOT_FMT, valid, mod_mode, delay_mode, reverb_mode,
                      *mod_norm, *delay_norm, *reverb_norm, *fx_enabled)
    assert len(raw) == 92
    return raw


def unpack_slot(raw: bytes) -> dict:
    assert len(raw) == 92
    f = struct.unpack(SLOT_FMT, raw)
    return {
        "valid":        f[0],
        "mod_mode":     f[1],
        "delay_mode":   f[2],
        "reverb_mode":  f[3],
        "mod_norm":     list(f[4:11]),
        "delay_norm":   list(f[11:18]),
        "reverb_norm":  list(f[18:25]),
        "fx_enabled":   list(f[25:28]),
    }


# ── SysEx frame builders ────────────────────────────────────────────────────

def sysex_get_status() -> list:
    return [MFG_ID, CMD_GET_STATUS]

def sysex_get_live() -> list:
    return [MFG_ID, CMD_GET_LIVE]

def sysex_get_preset(bank: int, slot: int) -> list:
    return [MFG_ID, CMD_GET_PRESET, bank, slot]

def sysex_put_preset(bank: int, slot: int, raw: bytes, name: str) -> list:
    name_b = (name.encode("ascii")[:11] + b"\x00" * 12)[:12]
    enc = encode7bit(raw)
    return [MFG_ID, CMD_PUT_PRESET, bank, slot] + list(name_b) + list(enc)

def sysex_set_mode(stage: int, index: int) -> list:
    return [MFG_ID, CMD_SET_MODE, stage, index]

def sysex_set_fx(stage: int, enabled: bool) -> list:
    return [MFG_ID, CMD_SET_FX, stage, int(enabled)]


# ── Device I/O ──────────────────────────────────────────────────────────────

class Device:
    def __init__(self, inport, outport):
        self._in  = inport
        self._out = outport

    def send_sysex(self, data: list):
        self._out.send(mido.Message("sysex", data=data))

    def send_cc(self, cc: int, value: int, channel: int = 0):
        self._out.send(mido.Message("control_change",
                                    channel=channel, control=cc, value=value))

    def wait_for(self, match_fn, timeout: float = TIMEOUT_S):
        """Poll until match_fn(msg.data) returns non-None or timeout expires."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self._in.receive(block=False)
            if msg and msg.type == "sysex" and len(msg.data) >= 3:
                if msg.data[0] == MFG_ID:
                    result = match_fn(msg.data)
                    if result is not None:
                        return result
            time.sleep(0.005)
        return None

    def wait_ack(self, cmd: int, timeout: float = TIMEOUT_S):
        def match(data):
            if data[1] == RESP_ACK and data[2] == cmd and len(data) >= 6:
                return {"ok": data[3] == 0x00, "bank": data[4], "slot": data[5]}
            return None
        return self.wait_for(match, timeout)

    def wait_preset_data(self, bank: int, slot: int, timeout: float = TIMEOUT_S):
        def match(data):
            if data[1] == RESP_PRESET and data[2] == bank and data[3] == slot:
                name = bytes(data[4:16]).rstrip(b"\x00").decode("ascii", errors="replace")
                raw  = decode7bit(bytes(data[16:-1]))
                if len(raw) == 92:
                    return {"name": name, "slot_data": unpack_slot(raw)}
            return None
        return self.wait_for(match, timeout)

    def wait_live_state(self, timeout: float = TIMEOUT_S):
        def match(data):
            if data[1] == RESP_LIVE and len(data) >= 5:
                raw = decode7bit(bytes(data[4:-1]))
                if len(raw) == 92:
                    return {"bank": data[2], "slot": data[3],
                            "slot_data": unpack_slot(raw)}
            return None
        return self.wait_for(match, timeout)


# ── Test cases ──────────────────────────────────────────────────────────────

class Results:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def section(self, name: str):
        print(f"\n[{name}]")

    def check(self, label: str, ok: bool, detail: str = ""):
        marker = "PASS" if ok else "FAIL"
        suffix = f"  ({detail})" if detail else ""
        print(f"  {marker}  {label}{suffix}")
        if ok:
            self.passed += 1
        else:
            self.failed += 1

    def summary(self) -> int:
        print(f"\n{'─' * 48}")
        print(f"Results: {self.passed} passed, {self.failed} failed")
        return 1 if self.failed else 0


def test_ping(dev: Device, r: Results):
    r.section("Ping (GET_STATUS)")
    dev.send_sysex(sysex_get_status())
    ack = dev.wait_ack(CMD_GET_STATUS)
    r.check("device responds within 1 s",      ack is not None)
    if ack:
        r.check("ACK ok flag is set",          ack["ok"])
        r.check("active bank in [0..9]",       0 <= ack["bank"] <= 9,
                f"bank={ack['bank']}")
        r.check("active slot in [0..9]",       0 <= ack["slot"] <= 9,
                f"slot={ack['slot']}")


def test_cc_live_state(dev: Device, r: Results):
    r.section("CC → live-state round-trip")
    cc_value = 64
    expected = cc_value / 127.0
    dev.send_cc(CC_MOD_BASE + 0, cc_value)
    time.sleep(CC_SETTLE_MS / 1000.0)
    dev.send_sysex(sysex_get_live())
    ack  = dev.wait_ack(CMD_GET_LIVE)
    r.check("GET_LIVE_STATE ACK received",     ack is not None)
    live = dev.wait_live_state()
    r.check("live-state data frame received",  live is not None)
    if live:
        got = live["slot_data"]["mod_norm"][0]
        r.check("mod_norm[0] matches CC 14",
                abs(got - expected) < 0.005,
                f"expected≈{expected:.3f} got={got:.3f}")
    # Restore mod param 0.
    dev.send_cc(CC_MOD_BASE + 0, 0)


def test_preset_roundtrip(dev: Device, r: Results):
    r.section(f"Preset round-trip  (bank {SCRATCH_BANK}, slot {SCRATCH_SLOT})")

    # Save original so we can restore later.
    dev.send_sysex(sysex_get_preset(SCRATCH_BANK, SCRATCH_SLOT))
    original = dev.wait_preset_data(SCRATCH_BANK, SCRATCH_SLOT)

    # Write a fully specified test preset.
    mod_n    = [round(0.1 * (i + 1), 6) for i in range(7)]
    delay_n  = [round(0.2 * (i + 1), 6) for i in range(7)]
    reverb_n = [round(0.3 * (i + 1), 6) for i in range(7)]
    raw_out  = pack_slot(valid=1, mod_mode=3, delay_mode=5, reverb_mode=2,
                         mod_norm=mod_n, delay_norm=delay_n,
                         reverb_norm=reverb_n, fx_enabled=[1, 0, 1])
    dev.send_sysex(sysex_put_preset(SCRATCH_BANK, SCRATCH_SLOT, raw_out, "HW-Test"))
    ack = dev.wait_ack(CMD_PUT_PRESET)
    r.check("PUT_PRESET ACK received",         ack is not None)
    r.check("PUT_PRESET ok=true",              ack is not None and ack["ok"])

    # Read it back.
    dev.send_sysex(sysex_get_preset(SCRATCH_BANK, SCRATCH_SLOT))
    rb = dev.wait_preset_data(SCRATCH_BANK, SCRATCH_SLOT)
    r.check("GET_PRESET response received",    rb is not None)
    if rb:
        s = rb["slot_data"]
        r.check("name round-trips",            rb["name"] == "HW-Test",
                f"got='{rb['name']}'")
        r.check("valid == 1",                  s["valid"] == 1)
        r.check("mod_mode == 3",               s["mod_mode"] == 3)
        r.check("delay_mode == 5",             s["delay_mode"] == 5)
        r.check("reverb_mode == 2",            s["reverb_mode"] == 2)
        r.check("fx_enabled == [1, 0, 1]",     s["fx_enabled"] == [1, 0, 1])
        r.check("mod_norm round-trips",
                all(abs(s["mod_norm"][i] - mod_n[i]) < 1e-5 for i in range(7)))
        r.check("delay_norm round-trips",
                all(abs(s["delay_norm"][i] - delay_n[i]) < 1e-5 for i in range(7)))
        r.check("reverb_norm round-trips",
                all(abs(s["reverb_norm"][i] - reverb_n[i]) < 1e-5 for i in range(7)))

    # Restore original (or clear if the slot was empty).
    if original:
        sd = original["slot_data"]
        raw_orig = pack_slot(valid=sd["valid"], mod_mode=sd["mod_mode"],
                             delay_mode=sd["delay_mode"], reverb_mode=sd["reverb_mode"],
                             mod_norm=sd["mod_norm"], delay_norm=sd["delay_norm"],
                             reverb_norm=sd["reverb_norm"], fx_enabled=sd["fx_enabled"])
        dev.send_sysex(sysex_put_preset(SCRATCH_BANK, SCRATCH_SLOT,
                                        raw_orig, original["name"]))
        dev.wait_ack(CMD_PUT_PRESET)


def test_set_mode(dev: Device, r: Results):
    r.section("SET_MODE")
    dev.send_sysex(sysex_set_mode(0, 1))
    ack = dev.wait_ack(CMD_SET_MODE)
    r.check("SET_MODE ACK received",           ack is not None)
    r.check("SET_MODE ok=true",                ack is not None and ack["ok"])
    dev.send_sysex(sysex_set_mode(0, 0))   # restore mode 0
    dev.wait_ack(CMD_SET_MODE)


def test_set_fx_enabled(dev: Device, r: Results):
    r.section("SET_FX_ENABLED")
    dev.send_sysex(sysex_set_fx(0, False))
    ack_off = dev.wait_ack(CMD_SET_FX)
    r.check("disable stage 0: ACK received",   ack_off is not None)
    r.check("disable stage 0: ok=true",        ack_off is not None and ack_off["ok"])
    dev.send_sysex(sysex_set_fx(0, True))
    ack_on = dev.wait_ack(CMD_SET_FX)
    r.check("re-enable stage 0: ACK received", ack_on is not None)
    r.check("re-enable stage 0: ok=true",      ack_on is not None and ack_on["ok"])


# ── Port selection ──────────────────────────────────────────────────────────

def find_port(names: list, hint: Optional[str]) -> Optional[str]:
    if hint:
        return next((n for n in names if hint.lower() in n.lower()), None)
    for kw in ("daisy", "multi-fx", "multi_fx", "multifx"):
        match = next((n for n in names if kw in n.lower()), None)
        if match:
            return match
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Multi-FX hardware-in-loop tests")
    parser.add_argument("--port", help="MIDI port name (substring match)")
    parser.add_argument("--list", action="store_true",
                        help="List available MIDI ports and exit")
    args = parser.parse_args()

    in_ports  = mido.get_input_names()
    out_ports = mido.get_output_names()

    if args.list:
        print("Input ports:")
        for p in in_ports:  print(f"  {p}")
        print("Output ports:")
        for p in out_ports: print(f"  {p}")
        return 0

    in_name  = find_port(in_ports,  args.port)
    out_name = find_port(out_ports, args.port)

    if not in_name or not out_name:
        print("ERROR: Daisy MIDI port not found.  Is the device connected and flashed?\n")
        print("Available input ports:")
        for p in in_ports:  print(f"  {p}")
        print("Available output ports:")
        for p in out_ports: print(f"  {p}")
        print("\nTip: use --port <substring>  or  --list  to inspect.")
        return 1

    print(f"IN : {in_name}")
    print(f"OUT: {out_name}")

    r = Results()
    with mido.open_input(in_name) as inp, mido.open_output(out_name) as out:
        dev = Device(inp, out)
        time.sleep(0.05)
        while inp.receive(block=False):   # drain stale messages
            pass

        test_ping(dev, r)
        test_cc_live_state(dev, r)
        test_preset_roundtrip(dev, r)
        test_set_mode(dev, r)
        test_set_fx_enabled(dev, r)

    return r.summary()


if __name__ == "__main__":
    sys.exit(main())
