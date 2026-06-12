#pragma once
#include <cstdint>

// Stub satisfying #include "daisy_seed.h" in source files compiled for host tests.

inline void SCB_InvalidateDCache_by_Addr(uint32_t*, int32_t) {}

namespace daisy {
    class QSPIHandle {
    public:
        int EraseSector(uint32_t) { return 0; }
        int Write(uint32_t, uint32_t, uint8_t*) { return 0; }
    };
} // namespace daisy
