#pragma once
#include "allpass_filter.h"

namespace pedal {

/// IIR allpass approximation of a Hilbert transformer.
///
/// Uses 4 first-order allpass sections per path.  The two paths produce
/// outputs that differ by ~90° across 60 Hz – 18 kHz at 48 kHz, giving
/// an analytic signal suitable for single-sideband frequency shifting.
///
/// Path A → "real"  output (constant group delay)
/// Path B → "imaginary" output (~90° relative to A)
///
/// Pole positions: Regalia-Mitra prototype, <1° phase error in band.
class HilbertTransform {
public:
    struct Frame { float re; float im; };

    void Init() {
        static constexpr float kA[4] = { 0.4776f, 0.7553f, 0.9285f, 0.9930f };
        static constexpr float kB[4] = { 0.1597f, 0.5577f, 0.8492f, 0.9810f };
        for (int i = 0; i < 4; ++i) {
            path_a_[i].SetCoeff(kA[i]);
            path_b_[i].SetCoeff(kB[i]);
        }
        Reset();
    }

    void Reset() {
        for (int i = 0; i < 4; ++i) {
            path_a_[i].Reset();
            path_b_[i].Reset();
        }
    }

    /// Returns {re, im} where im ≈ Hilbert-transform of re.
    Frame Process(float x) {
        float re = x;
        float im = x;
        for (int i = 0; i < 4; ++i) {
            re = path_a_[i].Process(re);
            im = path_b_[i].Process(im);
        }
        return {re, im};
    }

private:
    AllpassFilter path_a_[4];
    AllpassFilter path_b_[4];
};

} // namespace pedal
