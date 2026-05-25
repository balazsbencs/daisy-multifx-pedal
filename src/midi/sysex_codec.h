#pragma once
#include <cstdint>
#include <cstddef>

namespace pedal {

// Every 7 binary bytes become 8 SysEx-safe bytes: one MSB byte + 7 data bytes.
// All output bytes have bit 7 clear, as required by MIDI SysEx.

size_t Encode7bit(const uint8_t* in, size_t in_len, uint8_t* out);
size_t Decode7bit(const uint8_t* in, size_t in_len, uint8_t* out);

// Exact output size when encoding in_len binary bytes.
// Each group of 7 bytes → 8 bytes; last partial group of k → k+1 bytes.
constexpr size_t EncodedSize(size_t in_len) {
    const size_t full = in_len / 7u;
    const size_t rem  = in_len % 7u;
    return full * 8u + (rem > 0u ? rem + 1u : 0u);
}

} // namespace pedal
