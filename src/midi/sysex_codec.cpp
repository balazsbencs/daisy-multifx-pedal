#include "sysex_codec.h"

namespace pedal {

size_t Encode7bit(const uint8_t* in, size_t in_len, uint8_t* out) {
    size_t out_idx = 0;
    size_t in_idx  = 0;
    while (in_idx < in_len) {
        const size_t chunk = (in_len - in_idx < 7u) ? (in_len - in_idx) : 7u;
        uint8_t msb = 0;
        for (size_t i = 0; i < chunk; ++i) {
            if (in[in_idx + i] & 0x80u) msb |= static_cast<uint8_t>(1u << i);
        }
        out[out_idx++] = msb;
        for (size_t i = 0; i < chunk; ++i) {
            out[out_idx++] = in[in_idx + i] & 0x7Fu;
        }
        in_idx += chunk;
    }
    return out_idx;
}

size_t Decode7bit(const uint8_t* in, size_t in_len, uint8_t* out) {
    size_t out_idx = 0;
    size_t in_idx  = 0;
    while (in_idx < in_len) {
        const size_t remaining = in_len - in_idx;
        if (remaining < 2u) break;
        const size_t chunk = ((remaining - 1u) < 7u) ? (remaining - 1u) : 7u;
        const uint8_t msb  = in[in_idx++];
        for (size_t i = 0; i < chunk; ++i) {
            out[out_idx++] = in[in_idx++] | (((msb >> i) & 1u) << 7);
        }
    }
    return out_idx;
}

} // namespace pedal
