#pragma once

namespace pedal {

/// Fast sine approximation via a 5th-order polynomial.
///
/// Accuracy: max error ~0.5 % over a full cycle.  This is more than
/// sufficient for audio-rate LFOs and one-per-block filter coefficient
/// computation.  Using this instead of libm sinf saves ~1.5 KB of flash
/// because sinf.o (and cosf.o) are no longer pulled from the math library.
///
/// @param x  Phase in radians.  Expected range [0, 2π); values outside
///           this range give incorrect results.
inline float fast_sin(float x) noexcept {
    // Fold [π, 2π) down to [0, π), tracking the sign change.
    float sign = 1.0f;
    if (x > 3.14159265f) {
        x    -= 3.14159265f;
        sign  = -1.0f;
    }
    // Fold [π/2, π] to [0, π/2] using the identity sin(π − x) = sin(x).
    if (x > 1.57079633f) x = 3.14159265f - x;
    // 5th-order polynomial: x − x³/6 + x⁵/120, factored for fewer muls.
    const float x2 = x * x;
    return sign * x * (1.0f - x2 * (0.16666667f - x2 * 0.00833333f));
}

/// Fast cosine for x ∈ [0, π/2].
/// Uses the identity cos(x) = sin(π/2 − x).
/// Only valid for the stated range; used for the equal-power mix crossfade.
inline float fast_cos(float x) noexcept {
    return fast_sin(1.57079633f - x);
}

} // namespace pedal
