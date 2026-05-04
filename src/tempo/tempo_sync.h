#pragma once
#include "tap_tempo.h"
#include <cstdint>

namespace pedal {

/// Arbitrates between three tempo sources in priority order:
///   1. MIDI Clock (highest priority)
///   2. Tap Tempo
///   3. Pot (no override — returns -1)
///
/// MIDI clock timeout: if no beat edge is received for more than
/// MIDI_CLOCK_TIMEOUT_MS the MIDI source is considered inactive.
class TempoSync {
public:
    void Init();

    /// Call on every MIDI 0xF8 (Clock) message.
    /// @param now_ms Current time from System::GetNow().
    void OnMidiClock(uint32_t now_ms);

    /// Call when the tap button is pressed.
    /// @param now_ms Current time from System::GetNow().
    void OnTap(uint32_t now_ms);

    /// Call on MIDI 0xFC (Stop) to disable MIDI clock override.
    void OnMidiStop();

    /// Must be called every main loop so that MIDI clock timeout can be detected.
    /// @param now_ms Current time from System::GetNow().
    void Process(uint32_t now_ms);

    /// @return Delay period in seconds, or -1 if no tempo override is active.
    float GetOverrideSeconds() const;

    bool HasMidiClock() const { return midi_active_; }
    bool HasTap()       const { return tap_active_; }

private:
    TapTempo tap_tempo_;

    // Raw clock pulse tracking — 24 pulses per quarter note.
    static constexpr int      CLOCKS_PER_BEAT      = 24;
    // Declare MIDI clock stale after 2 beats have elapsed without a tick.
    static constexpr uint32_t MIDI_CLOCK_TIMEOUT_MS = 2000;

    uint32_t last_beat_ms_  = 0;  ///< Timestamp of the most recent beat edge.
    uint32_t last_tap_ms_   = 0;  ///< Timestamp of most recent tap event.
    uint32_t clock_count_   = 0;  ///< Pulses accumulated in the current beat.
    float    midi_period_s_ = -1.0f;
    bool     midi_active_   = false;
    bool     tap_active_    = false;
};

} // namespace pedal
