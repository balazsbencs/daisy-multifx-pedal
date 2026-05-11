#pragma once
#include "daisy_seed.h"

namespace pedal {
namespace pins {
// Encoders
constexpr daisy::Pin ENC_A         = daisy::seed::D0;
constexpr daisy::Pin ENC_B         = daisy::seed::D1;
constexpr daisy::Pin ENC_SW        = daisy::seed::D2;
constexpr daisy::Pin PARAM_ENC_0_A = daisy::seed::D7;
constexpr daisy::Pin PARAM_ENC_0_B = daisy::seed::D8;
constexpr daisy::Pin PARAM_ENC_1_A = daisy::seed::D9;
constexpr daisy::Pin PARAM_ENC_1_B = daisy::seed::D10;
constexpr daisy::Pin PARAM_ENC_2_A = daisy::seed::D27;
constexpr daisy::Pin PARAM_ENC_2_B = daisy::seed::D28;
constexpr daisy::Pin PARAM_ENC_3_A = daisy::seed::D29;
constexpr daisy::Pin PARAM_ENC_3_B = daisy::seed::D30;

// Footswitches (one per effect stage + tap)
constexpr daisy::Pin SW_FX_MOD    = daisy::seed::D3;
constexpr daisy::Pin SW_FX_DELAY  = daisy::seed::D4;
constexpr daisy::Pin SW_FX_REVERB = daisy::seed::D11;
constexpr daisy::Pin SW_TAP       = daisy::seed::D12;

// Status LEDs (one per effect stage)
constexpr daisy::Pin LED_FX_MOD    = daisy::seed::D6;
constexpr daisy::Pin LED_FX_DELAY  = daisy::seed::D15;
constexpr daisy::Pin LED_FX_REVERB = daisy::seed::D16;

// D5 is currently unassigned (was RELAY) — available for future use.

// ST7789 SPI display
constexpr daisy::Pin DISP_SCK = daisy::seed::D22;  // PA5  SPI1_SCK
constexpr daisy::Pin DISP_SDA = daisy::seed::D18;  // PA7  SPI1_MOSI
constexpr daisy::Pin DISP_CS  = daisy::seed::D13;  // PB6
constexpr daisy::Pin DISP_DC  = daisy::seed::D14;  // PB7
constexpr daisy::Pin DISP_RES = daisy::seed::D26;  // PD11
constexpr daisy::Pin DISP_BLK = daisy::seed::D24;  // PA1
} // namespace pins
} // namespace pedal
