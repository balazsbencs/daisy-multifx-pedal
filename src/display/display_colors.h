#pragma once
#include <cstdint>

namespace pedal {

// RGB565 colors in natural (host) byte order.
// The renderer swaps bytes on every write so SPI DMA delivers big-endian pixels.

constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorWhite = 0xFFFF;

// Per-mode accent colors (RGB565, natural order).
// Controlled by DISPLAY_MODE_COLORS compile flag; monochrome fallback otherwise.
#ifdef DISPLAY_MODE_COLORS
static constexpr uint16_t kModeColors[10] = {
    0x07FF,  // Digital  — cyan
    0xF800,  // Duck     — red
    0x07E0,  // Swell    — green
    0xFFE0,  // Trem     — yellow
    0xF81F,  // DBucket  — magenta
    0xFD20,  // Tape     — orange
    0x001F,  // Dual     — blue
    0x7FFF,  // Pattern  — light cyan
    0xFF00,  // Filter   — yellow-green
    0x8010,  // Lofi     — purple
};
#else
static constexpr uint16_t kModeColors[10] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
};
#endif

} // namespace pedal
