---
title: Tap & Hold
description: Tap tempo with MIDI-clock priority and reverb freeze (hold)
---

## Tap tempo

Tap the **TAP** footswitch in time with your tempo:

- 2–4 taps → averaged tempo, locked into the delay.
- BPM range 40–240; taps separated by more than 2 s reset the buffer.
- Tap-derived tempo overrides the TIME parameter on delay (and the
  SPEED parameter on modulation, when the mode supports tempo sync).
- Tap tempo defers to MIDI clock — if MIDI clock is being received,
  tap input is ignored.

## Hold (reverb freeze)

**Press and hold** the TAP switch for more than 500 ms while no MIDI
clock is active:

- Hold engages → "HOLD" appears in red on the status row, the reverb
  tail loops infinitely, dry passes through.
- Short-press TAP again to release hold (still registers as a tap).

This is meant for ambient swells — the reverb stage continues to take
input while held, so you can layer.

## Tempo arbitration

Three tempo sources compete; higher priority wins:

1. **MIDI Clock** (`0xF8`) — accumulates 24 ppq into a beat period.
   Stops following on `0xFC` or 2 s of silence.
2. **Tap tempo** — falls back when MIDI clock isn't running.
3. **Knob** — the TIME pot (delay) and SPEED pot (mod) are used when
   neither tap nor MIDI is providing a tempo override.

The active source is implicit: turn on MIDI clock and the delay locks
to the host. Stop the host and tap takes over. Tap timeout returns
control to the knob.
