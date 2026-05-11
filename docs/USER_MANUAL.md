# User Manual

How to actually play the Multi-FX pedal.

For wiring see [HARDWARE.md](HARDWARE.md). For source-tree orientation
see [README.md](../README.md).

---

## At a glance

Three independent effects in series:

```
IN ▶ MOD ▶ DELAY ▶ REVERB ▶ OUT (stereo)
```

Each stage has a dedicated **footswitch** (toggle on / off) and a
**status LED** (lit = engaged). On power-up all three stages are
**bypassed** — your input passes dry to the output until you tap a
footswitch.

---

## Front panel

| Control          | Function                                                       |
|------------------|----------------------------------------------------------------|
| **MOD** switch   | Toggle modulation stage on / off                               |
| **DLY** switch   | Toggle delay stage on / off                                    |
| **REV** switch   | Toggle reverb stage on / off                                   |
| **TAP** switch   | Tap tempo (short press) / hold (long press)                    |
| **MOD** LED      | Lit while MOD stage is engaged                                 |
| **DLY** LED      | Lit while DLY stage is engaged                                 |
| **REV** LED      | Lit while REV stage is engaged                                 |
| **Mode** encoder | Rotate: cycle modes within active page. Click: cycle pages. Hold + rotate: shift parameter encoders to params 5–7. |
| **P0..P3**       | Four parameter encoders — edit the four parameters shown for the active page. Hold the mode encoder to access params 5–7. |

---

## Display

```
┌──────────────────────────────────────────┐
│  [ MOD ] [ DLY ] [ REV ]                 │  ← page tabs (active highlighted)
│                                          │
│  Tape                       P1           │  ← active mode + preset slot
│ ──────────────────────────────────────── │
│  TIME       ████████░░░░░░               │
│  REPEATS    █████░░░░░░░░░░              │
│  MIX        ███████░░░░░░░░              │
│  FILTER     ████░░░░░░░░░░░░             │  ← seven parameter rows
│  GRIT       ██░░░░░░░░░░░░░░             │
│  MOD SPD    ███░░░░░░░░░░░░░             │
│  MOD DEP    █░░░░░░░░░░░░░░░             │
│ ──────────────────────────────────────── │
│  HOLD     120 BPM       SAVED            │  ← status row
│ ──────────────────────────────────────── │
│  MOD: Chorus  >  DLY: Tape  >  REV: Hall │  ← chain strip
└──────────────────────────────────────────┘
```

- The **tab strip** at the top shows which effect page you're editing.
  Active tab is filled with the page accent colour
  (cyan = MOD, orange = DLY, blue = REV).
- The **mode name** is the algorithm currently selected for the active
  page.
- The **chain strip** at the bottom shows all three stages at once.
  A disabled stage is dimmed and crossed-out.
- **Status row:** "HOLD" appears in red while reverb hold is active;
  "SAVED" / "LOAD" / "ERR" briefly flashes after a preset operation.

---

## Effect chain

Audio flows mono in → stereo out:

1. **Modulation** receives mono → outputs stereo. The dry path is
   passed mono through the wet/dry crossfade, so the stereo width comes
   entirely from the wet signal.
2. **Delay** takes the **left** channel from stage 1 as its mono input
   (echoes are stereo).
3. **Reverb** takes the left channel from stage 2 as its mono input
   (reverb tail is stereo).

A bypassed stage passes audio through untouched — no DSP work, no extra
latency.

The wet/dry mix on each stage uses constant-power crossfade
(equal-power sin / cos curves) with a normalisation factor that prevents
audible level dips at extreme mix settings.

---

## Modes

### Modulation page (6)

| Mode      | Description                                                     |
|-----------|-----------------------------------------------------------------|
| Chorus    | Classic stereo chorus, gentle pitch warble                      |
| Flanger   | Through-zero-style flanger with feedback                        |
| Rotary    | Two-rotor Leslie-style cabinet sim                              |
| Vibe      | 4-stage photo-cell phaser (uni-vibe character)                  |
| Phaser    | 6-stage smooth phaser                                           |
| VintTrem  | Tube-bias tremolo with optical curve                            |

### Delay page (4)

| Mode    | Description                                                      |
|---------|------------------------------------------------------------------|
| Digital | Clean digital delay, full-bandwidth repeats                      |
| Tape    | Tape echo with wow/flutter modulation, head-bump filtering       |
| Dual    | Two parallel delay lines (e.g. for ping-pong / dotted-eighth)    |
| FiltDly | Filter-feedback delay — repeats sweep through a moving filter    |

### Reverb page (4)

| Mode    | Description                                                      |
|---------|------------------------------------------------------------------|
| Room    | Small / medium room with early reflections                       |
| Hall    | Large concert-hall sustain                                       |
| Plate   | Bright EMT-style plate                                           |
| Spring  | Spring-tank emulation with characteristic boing                  |

---

## Parameters

Each page exposes seven parameters. The four physical encoders (P0–P3)
edit either parameters 1–4 (default) or parameters 5–7 (while you
hold down the mode encoder).

| Encoder        | Default param         | Shifted (mode held) |
|----------------|-----------------------|---------------------|
| P0             | param 1               | param 5             |
| P1             | param 2               | param 6             |
| P2             | param 3               | param 7             |
| P3             | param 4               | (unmapped)          |

Encoder behaviour:

- One detent = one step.
- Two detents within 40 ms = "fast mode" → step size jumps from
  1 % to 5 % per detent. Spin quickly to sweep, dial slowly to fine-tune.
- All parameters are clamped to [0, 1] internally and mapped to their
  per-mode physical range (e.g. delay TIME maps to 60 ms..2.5 s with a
  square-curve so the low end has more resolution).

### Modulation parameters

| Index | Label | Range / meaning                                   |
|-------|-------|---------------------------------------------------|
| 1     | SPEED | LFO rate, 0.05–10 Hz (square-curve)               |
| 2     | DEPTH | Modulation depth                                  |
| 3     | MIX   | Wet / dry                                         |
| 4     | TONE  | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)           |
| 5     | P1    | Mode-specific (e.g. feedback on Flanger/Vibe)     |
| 6     | P2    | Mode-specific                                     |
| 7     | LEVEL | Output gain, 0–2× (unity at 1.0)                  |

### Delay parameters

| Index | Label    | Range / meaning                                 |
|-------|----------|-------------------------------------------------|
| 1     | TIME     | Delay time, 60 ms – 2.5 s                       |
| 2     | REPEATS  | Feedback, 0 – 0.98                              |
| 3     | MIX      | Wet / dry                                       |
| 4     | FILTER   | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)         |
| 5     | GRIT     | Saturation / dirt amount                        |
| 6     | MOD SPD  | Modulation rate, 0.05–10 Hz                     |
| 7     | MOD DEP  | Modulation depth                                |

### Reverb parameters

| Index | Label    | Range / meaning                                       |
|-------|----------|-------------------------------------------------------|
| 1     | DECAY    | Reverb time, range varies per algo (0.2 s – 50 s)     |
| 2     | PRE DLY  | Pre-delay 0–500 ms                                    |
| 3     | MIX      | Wet / dry                                             |
| 4     | TONE     | Filter (0.5 = flat; < 0.5 LP; > 0.5 HP)               |
| 5     | MOD      | Modulation amount (chorus on the tail)                |
| 6     | PARAM1   | Per-algorithm                                         |
| 7     | PARAM2   | Per-algorithm                                         |

---

## Operating workflow

### Switching pages

Click the **mode encoder** to cycle through MOD → DLY → REV → MOD.
The active page tab highlights and the parameter rows redraw.

### Switching modes within a page

Once the page you want is active, **rotate the mode encoder**. The
mode name updates immediately, the new algorithm takes over the audio
path, and previous mode state is reset.

### Editing parameters

Rotate any of P0..P3. The bar in the corresponding row updates in
real time. To reach the last three parameters, **press and hold** the
mode encoder while you rotate.

### Toggling effect bypass

Stomp the corresponding footswitch (MOD / DLY / REV). The LED reflects
the new state, and the chain strip at the bottom of the display
highlights / dims the relevant section.

### Tap tempo

Tap the **TAP** footswitch in time with your tempo:

- 2–4 taps → averaged tempo, locked into the delay.
- BPM range 40–240; taps separated by more than 2 s reset the buffer.
- Tap-derived tempo overrides the TIME parameter on delay (and the
  SPEED parameter on modulation, when the mode supports tempo sync).
- Tap tempo defers to MIDI clock — if MIDI clock is being received,
  tap input is ignored.

### Hold (reverb freeze)

**Press and hold** the TAP switch for more than 500 ms while no MIDI
clock is active:

- Hold engages → "HOLD" appears in red on the status row, the reverb
  tail loops infinitely, dry passes through.
- Short-press TAP again to release hold (still registers as a tap).

This is meant for ambient swells — the reverb stage continues to take
input while held, so you can layer.

---

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

---

## MIDI

The pedal listens on **USB MIDI** (always) and **5-pin DIN UART MIDI**
(if you've wired the alternate hardware path).

### Control Change (CC)

Each parameter is mapped to a CC. Values 0–127 are normalised to
[0, 1] and overwrite the corresponding parameter:

| CC range  | Stage              |
|-----------|--------------------|
| 14 – 20   | Modulation params  |
| 21 – 27   | Delay params       |
| 28 – 34   | Reverb params      |
| 65        | Reverb hold (0/1)  |

CC 65 ≥ 64 → hold ON, < 64 → hold OFF.

### Realtime

| Message     | Effect                                  |
|-------------|-----------------------------------------|
| `0xF8` Clock | Drive tempo (24 ppq, 2 s timeout)      |
| `0xFC` Stop  | Release tempo override                 |

### Program Change

The handler decodes the byte but the main loop currently ignores it
(reserved for preset recall — see below).

---

## Presets

The firmware ships with a persistent storage layer in QSPI flash for
**8 preset slots** (slot label "P1".."P8" shown top-right of the
display). Each slot stores:

- The selected mode for each of the three stages
- All seven parameters per stage (21 floats total)
- A `valid` flag

The storage layer is initialised at boot and the slot label is rendered,
but **save / load UI bindings are not yet wired in this firmware
revision** — the slot stays at P1 and existing slots cannot yet be
recalled from the front panel. This is the next planned UI feature.

If you want to use presets today, drive `MultiPresetManager::SaveSlot`
and `LoadSlot` from custom code, or wait for the encoder long-press
binding.

---

## Power-on defaults

| Stage       | Mode    | Notable defaults |
|-------------|---------|----------------------------|
| Modulation  | Chorus  | Speed ≈ 1.5 Hz, Depth 0.5, Mix 0.5 |
| Delay       | Tape    | Time ≈ 600 ms, Repeats 0.4, Mix 0.5 |
| Reverb      | Hall    | Decay short, Pre-delay 20 ms, Mix 0.5 |

All three stages are **bypassed** at power-up. Stomp the relevant
footswitch to engage.

---

## Troubleshooting

| Symptom                        | Likely cause / fix                                           |
|--------------------------------|--------------------------------------------------------------|
| Display blank                  | UART MIDI was init **after** the display — see HARDWARE.md, MIDI conflict. |
| Footswitch does nothing        | Switch is latching; firmware expects momentary.              |
| Tap tempo not tracked          | MIDI clock is being received and outranks tap. Stop the host. |
| Knob has no effect on TIME     | Tempo override (tap or MIDI) is active. Wait 2 s after stopping the source. |
| Encoder skips / counts twice   | Use a 24-detent EC11; cheaper variants have inconsistent quadrature. |
| Audio breaks up at extreme MIX | Constant-power normalisation already applied. Check input gain. |
| LED stays off after stomp      | Daisy GPIO can only source ~2 mA — use a low-current LED or buffer it. |

For deeper diagnosis, add `hw.PrintLine(...)` near the failure point and
attach a serial console to the Daisy's USB device port (`screen /dev/cu.usbmodem* 115200`).

---

## Quick reference card

```
MODE ENCODER    click  → next page (MOD / DLY / REV)
                turn   → next algorithm in active page
                hold   → shift P0..P3 to params 5..7

P0..P3          turn   → edit parameter
                fast   → 5% step (two detents within 40ms)
                slow   → 1% step

FX FOOTSWITCH   stomp  → toggle stage bypass

TAP FOOTSWITCH  tap    → tap tempo
                hold   → engage reverb hold (long press > 500ms)
                tap    → release hold (also taps)

MIDI            USB    → always on
                CC 14+ → params (mod / delay / reverb in 7-CC blocks)
                CC 65  → hold
                Clock  → tempo (overrides tap)
```
