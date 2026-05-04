#pragma once

namespace pedal {

enum class BypassState { Bypassed, Active };

class Bypass {
public:
    void Init();
    void Toggle();
    bool IsActive() const { return state_ == BypassState::Active; }
    BypassState state() const { return state_; }

private:
    BypassState state_ = BypassState::Bypassed;
};

} // namespace pedal
