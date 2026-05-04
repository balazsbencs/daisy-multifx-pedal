#pragma once
#include "delay_line_sdram.h"
#include "../audio/stereo_frame.h"
#include <cstddef>

namespace pedal {

// Feedback Delay Network: 4-line or 8-line with Hadamard mixing.
// Buffers must be DSY_SDRAM_BSS arrays owned by the caller.
class Fdn {
public:
    static constexpr int MAX_LINES = 8;

    struct Config {
        int    n_lines;                  // 4 or 8
        float* bufs[MAX_LINES];          // per-line SDRAM buffers
        size_t delays[MAX_LINES];        // per-line delay in samples
        float  sample_rate;
    };

    void Init(const Config& cfg);
    void Reset();

    // Sets per-line feedback gains for the target RT60 decay time.
    void SetDecay(float decay_s);

    // One-pole LP coefficient in feedback path (0=none, higher=more damping).
    void SetDamping(float damp);

    // Per-line LFO modulation depth in samples (0 = no modulation).
    void SetModulation(float depth_samples);

    // Hold: freeze feedback gains at 1.0 (infinite sustain).
    void SetHold(bool hold);

    StereoFrame Process(float input);

private:
    void hadamard4(float v[4]) const;
    void hadamard8(float v[8]) const;

    DelayLineSdram lines_[MAX_LINES];
    float          delay_s_[MAX_LINES]{};        // per-line delay in seconds
    float          delay_samples_[MAX_LINES]{};  // per-line delay in samples (for modulated ReadAt)
    float          feedback_[MAX_LINES]{};       // nominal feedback gains
    float          lp_state_[MAX_LINES]{};       // one-pole LP state
    float          lfo_phase_[MAX_LINES]{};
    float          damp_        = 0.3f;
    float          mod_depth_   = 0.0f;
    float          sample_rate_ = 48000.0f;
    int            n_lines_     = 4;
    bool           hold_        = false;
};

} // namespace pedal
