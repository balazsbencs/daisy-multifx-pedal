#pragma once

namespace daisy {

class ScopedIrqBlocker {
public:
    ScopedIrqBlocker() {}
    ~ScopedIrqBlocker() = default;
};

} // namespace daisy
