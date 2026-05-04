#pragma once

namespace pedal {

class DcBlocker {
public:
    void Init() { x1_ = 0.0f; y1_ = 0.0f; }

    inline float Process(float x) {
        // y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
        float y = x - x1_ + 0.995f * y1_;
        x1_ = x;
        y1_ = y;
        return y;
    }

private:
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};

} // namespace pedal
