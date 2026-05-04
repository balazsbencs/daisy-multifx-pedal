#include "tempo_sync.h"

namespace pedal {

void TempoSync::Init() {
    tap_tempo_.Init();
    midi_active_  = false;
    tap_active_   = false;
    clock_count_  = 0;
    last_beat_ms_ = 0;
    last_tap_ms_  = 0;
    midi_period_s_ = -1.0f;
}

void TempoSync::OnMidiClock(uint32_t now_ms) {
    ++clock_count_;
    if (clock_count_ < static_cast<uint32_t>(CLOCKS_PER_BEAT)) {
        return;
    }
    clock_count_ = 0;

    // Measure the gap between consecutive beat edges.
    if (last_beat_ms_ > 0) {
        const uint32_t gap_ms = now_ms - last_beat_ms_;
        // Accept periods between 15 bpm (4 s) and 300 bpm (0.2 s).
        if (gap_ms >= 200 && gap_ms <= 4000) {
            midi_period_s_ = static_cast<float>(gap_ms) * 0.001f;
            midi_active_   = true;
        }
    }
    last_beat_ms_ = now_ms;
}

void TempoSync::OnTap(uint32_t now_ms) {
    const float bpm = tap_tempo_.Tap(now_ms);
    if (bpm > 0.0f) {
        tap_active_ = true;
        last_tap_ms_ = now_ms;
    }
}

void TempoSync::OnMidiStop() {
    midi_active_   = false;
    midi_period_s_ = -1.0f;
    clock_count_   = 0;
    last_beat_ms_  = 0;
}

void TempoSync::Process(uint32_t now_ms) {
    // Expire MIDI clock when no beat edge has arrived within the timeout window.
    if (midi_active_ && last_beat_ms_ > 0) {
        const uint32_t silence = now_ms - last_beat_ms_;
        if (silence > MIDI_CLOCK_TIMEOUT_MS) {
            midi_active_   = false;
            midi_period_s_ = -1.0f;
        }
    }

    // Expire tap override when taps go stale.
    if (tap_active_ && last_tap_ms_ > 0) {
        const uint32_t tap_silence = now_ms - last_tap_ms_;
        if (tap_silence > TAP_TIMEOUT_MS) {
            tap_active_ = false;
        }
    }
}

float TempoSync::GetOverrideSeconds() const {
    if (midi_active_) {
        return midi_period_s_;
    }
    if (tap_active_) {
        return tap_tempo_.GetPeriodSeconds();
    }
    return -1.0f;
}

} // namespace pedal
