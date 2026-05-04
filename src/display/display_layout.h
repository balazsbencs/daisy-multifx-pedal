#pragma once
#include <cstdint>

namespace pedal {
namespace layout {

constexpr uint16_t SCREEN_W = 240;
constexpr uint16_t SCREEN_H = 320;

// ── Header (y=0..39, h=40) ───────────────────────────────────────────────────
// Top half: three 80px tab buttons [MOD][DLY][REV]
// Bottom half: active mode name
constexpr uint16_t HEADER_H    = 40;
constexpr uint16_t TAB_H       = 20;
constexpr uint16_t TAB_W       = 80;   // 240 / 3
constexpr uint16_t TAB_TEXT_Y  = 5;    // Font_7x10 (10px) centred in 20px row
constexpr uint16_t MODE_Y      = 21;   // Font_11x18 (18px) in bottom half
constexpr uint16_t MODE_X      = 4;
constexpr uint16_t SLOT_X      = 192;
constexpr uint16_t SLOT_Y      = 24;   // Font_6x8 right-aligned

// ── Separator lines ──────────────────────────────────────────────────────────
constexpr uint16_t SEP1_Y = 39;
constexpr uint16_t SEP2_Y = 250;
constexpr uint16_t SEP3_Y = 280;

// ── Parameter rows (y=40..249, 7 × 30px) ────────────────────────────────────
constexpr uint16_t PARAM_AREA_Y   = 40;
constexpr uint16_t PARAM_ROW_H    = 30;
constexpr uint16_t LABEL_OFFSET_Y = 11;  // Font_6x8 (8px) vertically centred in 30px
constexpr uint16_t BAR_OFFSET_Y   = 9;
constexpr uint16_t BAR_H          = 14;
constexpr uint16_t LABEL_X        = 4;
constexpr uint16_t BAR_X          = 80;
constexpr uint16_t BAR_W          = 156;

// ── Status row (y=251..279) ──────────────────────────────────────────────────
constexpr uint16_t STATUS_Y      = 261;
constexpr uint16_t HOLD_X        = 4;
constexpr uint16_t TEMPO_X       = 64;
constexpr uint16_t PRESET_EVT_X  = 174;

// ── Chain strip (y=281..319, h=39) ───────────────────────────────────────────
constexpr uint16_t CHAIN_Y       = 281;
constexpr uint16_t CHAIN_H       = 39;
constexpr uint16_t CHAIN_SEC_W   = 80;   // 240 / 3
constexpr uint16_t CHAIN_TAG_Y   = 285;  // Font_6x8 (8px) — page label
constexpr uint16_t CHAIN_NAME_Y  = 298;  // Font_7x10 (10px) — mode name
constexpr uint16_t CHAIN_INNER_X = 4;    // left margin within each section
constexpr uint16_t CHAIN_ARR_X1  = 76;   // ">" between MOD and DLY sections
constexpr uint16_t CHAIN_ARR_X2  = 156;  // ">" between DLY and REV sections
constexpr uint16_t CHAIN_ARR_Y   = 298;

} // namespace layout
} // namespace pedal
