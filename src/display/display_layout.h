#pragma once
#include <cstdint>

namespace pedal {
namespace layout {

constexpr uint16_t SCREEN_W = 240;
constexpr uint16_t SCREEN_H = 320;

// ── Header (y=0..55, h=56) ────────────────────────────────────────────────────
constexpr uint16_t HEADER_H       = 56;

// Tab strip (y=0..27, h=28): three 80px tabs
constexpr uint16_t TAB_H          = 28;
constexpr uint16_t TAB_W          = 80;
constexpr uint16_t TAB_TEXT_Y     = 5;      // (28-18)/2: centre Font_11x18 (18px) in 28px
constexpr uint16_t TAB_TEXT_X_OFF = 23;     // (80-33)/2: centre "MOD"/"DLY"/"REV" (3×11px)

// Mode name (y=28..55, h=28)
constexpr uint16_t MODE_Y  = 33;            // (28-18)/2 + 28
constexpr uint16_t MODE_X  = 4;

// ── Separator lines ───────────────────────────────────────────────────────────
constexpr uint16_t SEP1_Y = 55;
constexpr uint16_t SEP2_Y = 231;   // after 4 × 44 px param rows
constexpr uint16_t SEP3_Y = 275;   // after preset indicator row

// ── Parameter rows (y=56..231, 4 × 44 px) ────────────────────────────────────
constexpr uint16_t PARAM_AREA_Y    = 56;
constexpr uint16_t PARAM_ROW_H     = 44;
constexpr uint16_t LABEL_OFFSET_Y  = 13;  // (44-18)/2: centre Font_11x18 in 44 px row
constexpr uint16_t BAR_OFFSET_Y    = 12;
constexpr uint16_t BAR_H           = 20;
constexpr uint16_t LABEL_X         = 4;
constexpr uint16_t BAR_X           = 88;
constexpr uint16_t BAR_W           = 148;

// ── Preset indicator row (y=232..275, h=44) ───────────────────────────────────
// Shows bank/slot in a large font; HOLD and event notices share the row.
constexpr uint16_t PRESET_ID_X    = 87;    // "B3 P07" — (240 - 6×11) / 2
constexpr uint16_t PRESET_ID_Y    = 245;   // vertically centred: 232 + (44-18)/2
constexpr uint16_t HOLD_X         = 4;
constexpr uint16_t HOLD_Y         = 264;   // Font_6x8 near bottom of row
constexpr uint16_t PRESET_EVT_X   = 174;
constexpr uint16_t PRESET_EVT_Y   = 264;

// ── Chain strip (y=276..319, h=44) ────────────────────────────────────────────
// Three sections: [MOD: <name>] > [DLY: <name>] > [REV: <name>]
// In browse mode "BROWSE" is drawn under the REV column.
constexpr uint16_t CHAIN_Y        = 276;
constexpr uint16_t CHAIN_H        = 44;
constexpr uint16_t CHAIN_SEC_W    = 80;
constexpr uint16_t CHAIN_TAG_Y    = 280;
constexpr uint16_t CHAIN_NAME_Y   = 296;
constexpr uint16_t CHAIN_INNER_X  = 4;
constexpr uint16_t CHAIN_ARR_X1   = 76;
constexpr uint16_t CHAIN_ARR_X2   = 156;
constexpr uint16_t CHAIN_ARR_Y    = 296;

// Preset browse indicator — drawn under the REV section of the chain strip
constexpr uint16_t BROWSE_IND_X   = 162;  // REV column start + 2
constexpr uint16_t BROWSE_IND_Y   = 309;  // 296 + 8 (name row height) + 5

} // namespace layout
} // namespace pedal
