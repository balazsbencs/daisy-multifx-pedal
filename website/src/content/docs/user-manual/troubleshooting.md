---
title: Troubleshooting
description: Common symptoms and fixes, plus the quick reference card
---

## Troubleshooting

| Symptom                        | Likely cause / fix                                                          |
|--------------------------------|-----------------------------------------------------------------------------|
| Display blank                  | UART MIDI was init **after** the display — see HARDWARE.md, MIDI conflict.  |
| Footswitch does nothing        | Switch is latching; firmware expects momentary.                             |
| Tap tempo not tracked          | MIDI clock is being received and outranks tap. Stop the host.               |
| Knob has no effect on TIME     | Tempo override (tap or MIDI) is active. Wait 2 s after stopping the source. |
| Encoder skips / counts twice   | Use a 24-detent EC11; cheaper variants have inconsistent quadrature.        |
| Audio breaks up at extreme MIX | Constant-power normalisation already applied. Check input gain.             |
| LED stays off after stomp      | Daisy GPIO can only source ~2 mA — use a low-current LED or buffer it.      |

For deeper diagnosis, add `hw.PrintLine(...)` near the failure point and
attach a serial console to the Daisy's USB device port (`screen /dev/cu.usbmodem* 115200`).

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
