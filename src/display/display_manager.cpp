#include "display_manager.h"
#include "display_renderer.h"
#include "display_colors.h"
#include "display_layout.h"
#include "../params/delay_param_map.h"
#include "../params/mod_param_map.h"
#include "../params/reverb_param_map.h"
#include "../params/param_range.h"
#include "util/oled_fonts.h"

namespace pedal {

// Per-page accent colors: MOD=cyan, DLY=orange, REV=blue
static constexpr uint16_t kPageAccent[3] = { 0x07FF, 0xFD20, 0x001F };

// ── Init ─────────────────────────────────────────────────────────────────────

void DisplayManager::Init() {
    driver_.Init();
    DisplayRenderer::Clear(kColorBlack);
    driver_.FillScreen(kColorBlack);
}

// ── Update ───────────────────────────────────────────────────────────────────

void DisplayManager::Update(int           active_page,
                            ModModeId     mod_mode,
                            DelayModeId   delay_mode,
                            ReverbModeId  reverb_mode,
                            const mod_fx::ParamSet&    mod_params,
                            const delay_fx::ParamSet&  delay_params,
                            const reverb_fx::ParamSet& reverb_params,
                            bool          hold_active,
                            int           preset_slot,
                            PresetUiEvent preset_event,
                            uint32_t      now_ms) {
    (void)now_ms;
    if (driver_.IsBusy()) return;

    Render(active_page, mod_mode, delay_mode, reverb_mode,
           mod_params, delay_params, reverb_params,
           hold_active, preset_slot, preset_event);

    driver_.StartDmaTransfer(DisplayRenderer::FrameBuffer(),
                             DisplayRenderer::FrameBufferBytes(),
                             nullptr, nullptr);
}

// ── Render ───────────────────────────────────────────────────────────────────

void DisplayManager::Render(int           active_page,
                            ModModeId     mod_mode,
                            DelayModeId   delay_mode,
                            ReverbModeId  reverb_mode,
                            const mod_fx::ParamSet&    mod_params,
                            const delay_fx::ParamSet&  delay_params,
                            const reverb_fx::ParamSet& reverb_params,
                            bool          hold_active,
                            int           preset_slot,
                            PresetUiEvent preset_event) {
    const uint16_t accent = kPageAccent[active_page];

    DisplayRenderer::Clear(kColorBlack);

    // ── Header: three tab buttons (y=0..19) ──────────────────────────────────
    static const char* kTabLabels[3] = { "MOD", "DLY", "REV" };
    for (int t = 0; t < 3; ++t) {
        const uint16_t tx = static_cast<uint16_t>(t * layout::TAB_W);
        if (t == active_page) {
            DisplayRenderer::FillRect(tx, 0,
                                      layout::TAB_W, layout::TAB_H,
                                      kPageAccent[t]);
            DisplayRenderer::DrawText(tx + 26, layout::TAB_TEXT_Y,
                                      kTabLabels[t],
                                      kColorBlack, kPageAccent[t], Font_7x10);
        } else {
            DisplayRenderer::DrawText(tx + 26, layout::TAB_TEXT_Y,
                                      kTabLabels[t],
                                      kPageAccent[t], kColorBlack, Font_7x10);
        }
    }

    // ── Header: active mode name (y=20..39) ──────────────────────────────────
    const char* mode_name = (active_page == 0) ? ModModeName(mod_mode) :
                            (active_page == 1) ? DelayModeName(delay_mode) :
                            ReverbModeName(reverb_mode);
    DisplayRenderer::DrawText(layout::MODE_X, layout::MODE_Y,
                              mode_name, accent, kColorBlack, Font_7x10);

    // Preset slot label "P1".."P8"
    char slot_buf[3] = { 'P', static_cast<char>('1' + (preset_slot % 8)), 0 };
    DisplayRenderer::DrawText(layout::SLOT_X, layout::SLOT_Y,
                              slot_buf, kColorWhite, kColorBlack, Font_6x8);

    // ── Separator lines ───────────────────────────────────────────────────────
    DisplayRenderer::HLine(0, layout::SEP1_Y, layout::SCREEN_W, kColorWhite);
    DisplayRenderer::HLine(0, layout::SEP2_Y, layout::SCREEN_W, kColorWhite);
    DisplayRenderer::HLine(0, layout::SEP3_Y, layout::SCREEN_W, kColorWhite);

    // ── Parameter rows ────────────────────────────────────────────────────────
    static const char* kModLabels[7]   = { "SPEED", "DEPTH", "MIX",
                                           "TONE",  "P1",    "P2",    "LEVEL" };
    static const char* kDelayLabels[7] = { "TIME",  "REPEATS", "MIX",
                                           "FILTER","GRIT","MOD SPD","MOD DEP" };
    static const char* kReverbLabels[7]= { "DECAY", "PRE DLY", "MIX",
                                           "TONE",  "MOD", "PARAM1", "PARAM2" };

    const char** labels = (active_page == 0) ? kModLabels :
                          (active_page == 1) ? kDelayLabels : kReverbLabels;

    // Compute bar fill values (normalized [0,1])
    float bar[7];
    if (active_page == 0) {
        using namespace mod_fx;
        bar[0] = unmap_param(mod_params.speed, get_param_range(mod_mode, ParamId::Speed));
        bar[1] = mod_params.depth;
        bar[2] = mod_params.mix;
        bar[3] = mod_params.tone;
        bar[4] = mod_params.p1;
        bar[5] = mod_params.p2;
        bar[6] = unmap_param(mod_params.level, default_ranges::LEVEL);
    } else if (active_page == 1) {
        using namespace delay_fx;
        bar[0] = unmap_param(delay_params.time,    get_param_range(delay_mode, ParamId::Time));
        bar[1] = unmap_param(delay_params.repeats, default_ranges::REPEATS);
        bar[2] = delay_params.mix;
        bar[3] = delay_params.filter;
        bar[4] = delay_params.grit;
        bar[5] = unmap_param(delay_params.mod_spd, default_ranges::MOD_SPD);
        bar[6] = delay_params.mod_dep;
    } else {
        using namespace reverb_fx;
        bar[0] = unmap_param(reverb_params.decay,     get_param_range(reverb_mode, ParamId::Decay));
        bar[1] = unmap_param(reverb_params.pre_delay, default_ranges::PRE_DELAY);
        bar[2] = reverb_params.mix;
        bar[3] = reverb_params.tone;
        bar[4] = reverb_params.mod;
        bar[5] = unmap_param(reverb_params.param1,    get_param_range(reverb_mode, ParamId::Param1));
        bar[6] = unmap_param(reverb_params.param2,    get_param_range(reverb_mode, ParamId::Param2));
    }

    for (int i = 0; i < 7; ++i) {
        const uint16_t row_y = static_cast<uint16_t>(
            layout::PARAM_AREA_Y + static_cast<uint16_t>(i) * layout::PARAM_ROW_H);

        DisplayRenderer::DrawText(layout::LABEL_X,
                                  row_y + layout::LABEL_OFFSET_Y,
                                  labels[i], kColorWhite, kColorBlack, Font_6x8);

        DisplayRenderer::DrawBar(layout::BAR_X,
                                 row_y + layout::BAR_OFFSET_Y,
                                 layout::BAR_W, layout::BAR_H,
                                 bar[i], accent);
    }

    // ── Status row ────────────────────────────────────────────────────────────
    if (hold_active) {
        DisplayRenderer::DrawText(layout::HOLD_X, layout::STATUS_Y,
                                  "HOLD", 0xF800, kColorBlack, Font_6x8);
    }
    if (preset_event == PresetUiEvent::Saved) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "SAVED", kColorWhite, kColorBlack, Font_6x8);
    } else if (preset_event == PresetUiEvent::Loaded) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "LOAD", kColorWhite, kColorBlack, Font_6x8);
    } else if (preset_event == PresetUiEvent::Error) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "ERR", 0xF800, kColorBlack, Font_6x8);
    }

    // ── Chain strip (y=281..319) ──────────────────────────────────────────────
    // Three sections: [MOD: <name>] > [DLY: <name>] > [REV: <name>]
    // Section 0 (MOD)
    DisplayRenderer::DrawText(0 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_TAG_Y,
                              "MOD", kPageAccent[0], kColorBlack, Font_6x8);
    DisplayRenderer::DrawText(0 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_NAME_Y,
                              ModModeName(mod_mode),
                              (active_page == 0) ? kPageAccent[0] : kColorWhite,
                              kColorBlack, Font_6x8);

    // Arrow 1
    DisplayRenderer::DrawText(layout::CHAIN_ARR_X1, layout::CHAIN_ARR_Y,
                              ">", kColorWhite, kColorBlack, Font_6x8);

    // Section 1 (DLY)
    DisplayRenderer::DrawText(1 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_TAG_Y,
                              "DLY", kPageAccent[1], kColorBlack, Font_6x8);
    DisplayRenderer::DrawText(1 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_NAME_Y,
                              DelayModeName(delay_mode),
                              (active_page == 1) ? kPageAccent[1] : kColorWhite,
                              kColorBlack, Font_6x8);

    // Arrow 2
    DisplayRenderer::DrawText(layout::CHAIN_ARR_X2, layout::CHAIN_ARR_Y,
                              ">", kColorWhite, kColorBlack, Font_6x8);

    // Section 2 (REV)
    DisplayRenderer::DrawText(2 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_TAG_Y,
                              "REV", kPageAccent[2], kColorBlack, Font_6x8);
    DisplayRenderer::DrawText(2 * layout::CHAIN_SEC_W + layout::CHAIN_INNER_X,
                              layout::CHAIN_NAME_Y,
                              ReverbModeName(reverb_mode),
                              (active_page == 2) ? kPageAccent[2] : kColorWhite,
                              kColorBlack, Font_6x8);
}

// ── Name tables ───────────────────────────────────────────────────────────────

const char* DisplayManager::ModModeName(ModModeId id) {
    switch (id) {
        case ModModeId::Chorus:  return "Chorus";
        case ModModeId::Flanger: return "Flanger";
        case ModModeId::Rotary:  return "Rotary";
        case ModModeId::Vibe:    return "Vibe";
        case ModModeId::Phaser:  return "Phaser";
        case ModModeId::VintTrem: return "VintTrem";
        default: return "?";
    }
}

const char* DisplayManager::DelayModeName(DelayModeId id) {
    switch (id) {
        case DelayModeId::Digital: return "Digital";
        case DelayModeId::Tape:    return "Tape";
        case DelayModeId::Dual:    return "Dual";
        case DelayModeId::Filter:  return "FiltDly";
        default: return "?";
    }
}

const char* DisplayManager::ReverbModeName(ReverbModeId id) {
    switch (id) {
        case ReverbModeId::Room:   return "Room";
        case ReverbModeId::Hall:   return "Hall";
        case ReverbModeId::Plate:  return "Plate";
        case ReverbModeId::Spring: return "Spring";
        default: return "?";
    }
}

} // namespace pedal
