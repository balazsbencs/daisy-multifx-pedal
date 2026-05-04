#include "bypass.h"

namespace pedal {

void Bypass::Init() {
    state_ = BypassState::Active;
}

void Bypass::Toggle() {
    state_ = (state_ == BypassState::Active)
        ? BypassState::Bypassed
        : BypassState::Active;
}

} // namespace pedal
