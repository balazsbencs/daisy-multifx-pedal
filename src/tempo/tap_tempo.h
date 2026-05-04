#pragma once
#include <cstdint>
#include "../config/constants.h"

namespace pedal {

/// Accumulates tap timestamps and computes average BPM.
/// Resets automatically when a tap gap exceeds TAP_TIMEOUT_MS.
class TapTempo {
public:
    void Init();

    /// Call when the tap button is pressed.
    /// @param now_ms Current time from System::GetNow().
    /// @return Computed BPM, or 0 if fewer than 2 taps have been recorded.
    float Tap(uint32_t now_ms);

    float GetBpm() const { return bpm_; }

    /// Returns the beat period in seconds (60 / bpm).
    float GetPeriodSeconds() const;

private:
    uint32_t tap_times_[TAP_MAX_TAPS]{};
    int      tap_count_ = 0;
    float    bpm_       = 120.0f;
};

} // namespace pedal
