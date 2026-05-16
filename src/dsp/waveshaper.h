#pragma once

namespace pedal {

enum class WaveCurve { SoftClip, Tape, Tube };

class WaveShaper {
public:
    void Init(WaveCurve curve = WaveCurve::SoftClip) {
        curve_ = curve;
        drive_ = 1.0f;
    }
    void SetDrive(float drive) { drive_ = 1.0f + drive * 15.0f; }
    float Process(float x) const;

private:
    WaveCurve curve_ = WaveCurve::SoftClip;
    float     drive_ = 1.0f;
};

inline float WaveShaper::Process(float x) const {
    float driven = x * drive_;
    switch (curve_) {
        case WaveCurve::SoftClip: {
            if (driven >  1.0f) driven =  1.0f;
            if (driven < -1.0f) driven = -1.0f;
            return driven * (3.0f - driven * driven) * (1.0f / 3.0f);
        }
        case WaveCurve::Tape: {
            // Rational soft-limiter: x/(1+|x|), output in (-1,+1) without hard clip.
            // Warmer and smoother than the cubic; no pre-clamp needed.
            const float abs_d = driven >= 0.0f ? driven : -driven;
            return driven / (1.0f + abs_d);
        }
        case WaveCurve::Tube: {
            // Small positive bias before cubic -> asymmetric clipping -> 2nd harmonic.
            // DC offset is removed by each mode's DcBlocker.
            driven += 0.1f;
            if (driven >  1.0f) driven =  1.0f;
            if (driven < -1.0f) driven = -1.0f;
            return driven * (3.0f - driven * driven) * (1.0f / 3.0f);
        }
        default:
            return driven;
    }
}

} // namespace pedal
