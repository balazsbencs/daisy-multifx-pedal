---
title: MIDI & Tempo
description: USB + UART MIDI handler, CC dispatch, MIDI clock, tap tempo arbitration
---

## Tempo Priority Chain

`TempoSync` arbitrates the `time`/`speed` parameter across all three stages:
1. **MIDI Clock** (highest) — locks to beat period; expires 2 s after last tick
2. **Tap Tempo** — averaging up to 4 taps; 2 s timeout
3. **Encoder / MIDI CC** — normal control

## Control Change (CC)

Each parameter is mapped to a CC. Values 0–127 are normalised to `[0, 1]` and overwrite the
corresponding parameter:

| CC range  | Stage              |
|-----------|--------------------|
| 14 – 20   | Modulation params  |
| 21 – 27   | Delay params       |
| 28 – 34   | Reverb params      |
| 65        | Reverb hold (0/1)  |

CC 65 ≥ 64 → hold ON, < 64 → hold OFF.

## Realtime messages

| Message      | Effect                                  |
|--------------|-----------------------------------------|
| `0xF8` Clock | Drive tempo (24 ppq, 2 s timeout)       |
| `0xFC` Stop  | Release tempo override                  |

## Implementation notes

- USB MIDI is handled by libDaisy's native USB device stack — no extra wiring
- UART MIDI RX is on D14 (shared with display DC after init — see [Display](./display/) for the constraint)
- CC values 0–127 are normalised to [0, 1] in `MidiHandler::HandleCC()` and written directly into
  the main loop's normalization arrays (`mod_norm[]`, `delay_norm[]`, `reverb_norm[]`)
- MIDI clock (`0xF8`) accumulates 24 pulses-per-quarter-note into a beat period in `TempoSync`
- Clock expires 2 s after the last tick or on `0xFC` (Stop)

## Source layout

MIDI and tempo code lives in `src/midi/` and `src/tempo/`:

| File | Responsibility |
|------|---------------|
| `midi/midi_handler.h/.cpp` | Parses USB + UART MIDI byte streams; dispatches CC, Note, Clock, Program Change |
| `tempo/tempo_sync.h/.cpp` | Tap accumulator, MIDI clock integrator, priority arbitration, BPM → period conversion |

## Tap tempo detail

The tap accumulator stores up to 4 inter-tap intervals. Each new tap appends the elapsed time since
the last tap; the running average is recomputed and pushed into `TempoSync`. Taps more than 2 s
apart reset the accumulator. The resulting period overrides only while no MIDI clock has been
received within 2 s; after that timeout the encoder / CC value resumes control.

Long-pressing the TAP footswitch (> 500 ms) while MIDI clock is not active engages reverb hold
instead of registering a tap.
