---
title: Signal Chain
description: How audio flows through the three effect stages
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
