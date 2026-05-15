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

// Mode name + preset slot (y=28..55, h=28)
constexpr uint16_t MODE_Y  = 33;            // (28-18)/2 + 28
constexpr uint16_t MODE_X  = 4;
constexpr uint16_t SLOT_X  = 200;
constexpr uint16_t SLOT_Y  = 33;

// ── Separator lines ───────────────────────────────────────────────────────────
constexpr uint16_t SEP1_Y = 55;
constexpr uint16_t SEP2_Y = 279;
constexpr uint16_t SEP3_Y = 291;

// ── Parameter rows (y=56..279, 4 × 56px) ─────────────────────────────────────
constexpr uint16_t PARAM_AREA_Y    = 56;
constexpr uint16_t PARAM_ROW_H     = 56;
constexpr uint16_t LABEL_OFFSET_Y  = 8;    // Font_11x18 (18px) starts here within row
constexpr uint16_t BAR_OFFSET_Y    = 30;   // bar/enum text starts here within row
constexpr uint16_t BAR_H           = 20;
constexpr uint16_t LABEL_X         = 4;
constexpr uint16_t BAR_X           = 88;
constexpr uint16_t BAR_W           = 148;

// ── Status row (y=280..291, h=12) ─────────────────────────────────────────────
constexpr uint16_t STATUS_Y       = 284;
constexpr uint16_t HOLD_X         = 4;
constexpr uint16_t PRESET_EVT_X   = 174;

// ── Chain strip (y=292..319, h=28) ────────────────────────────────────────────
constexpr uint16_t CHAIN_Y        = 292;
constexpr uint16_t CHAIN_H        = 28;
constexpr uint16_t CHAIN_SEC_W    = 80;
constexpr uint16_t CHAIN_TAG_Y    = 294;
constexpr uint16_t CHAIN_NAME_Y   = 306;
constexpr uint16_t CHAIN_INNER_X  = 4;
constexpr uint16_t CHAIN_ARR_X1   = 76;
constexpr uint16_t CHAIN_ARR_X2   = 156;
constexpr uint16_t CHAIN_ARR_Y    = 306;

} // namespace layout
} // namespace pedal
