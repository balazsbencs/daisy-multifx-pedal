#include "tap_tempo.h"

namespace pedal {

void TapTempo::Init() {
    tap_count_ = 0;
    bpm_       = 120.0f;
}

float TapTempo::Tap(uint32_t now_ms) {
    // Reset the sequence when the gap since the last tap is too long.
    if (tap_count_ > 0) {
        const uint32_t gap = now_ms - tap_times_[tap_count_ - 1];
        if (gap > TAP_TIMEOUT_MS) {
            tap_count_ = 0;
        }
    }

    // Record the tap timestamp, keeping at most TAP_MAX_TAPS entries.
    if (tap_count_ < TAP_MAX_TAPS) {
        tap_times_[tap_count_++] = now_ms;
    } else {
        // Shift the window left and append the new tap at the end.
        for (int i = 0; i < TAP_MAX_TAPS - 1; ++i) {
            tap_times_[i] = tap_times_[i + 1];
        }
        tap_times_[TAP_MAX_TAPS - 1] = now_ms;
    }

    if (tap_count_ < 2) {
        return 0.0f;
    }

    // Average the inter-tap intervals across all recorded taps.
    const uint32_t total_gap = tap_times_[tap_count_ - 1] - tap_times_[0];
    const float    avg_ms    = static_cast<float>(total_gap)
                             / static_cast<float>(tap_count_ - 1);
    const float new_bpm = 60000.0f / avg_ms;

    if (new_bpm >= TAP_MIN_BPM && new_bpm <= TAP_MAX_BPM) {
        bpm_ = new_bpm;
    }
    return bpm_;
}

float TapTempo::GetPeriodSeconds() const {
    return 60.0f / bpm_;
}

} // namespace pedal
