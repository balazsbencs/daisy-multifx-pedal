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

static constexpr uint16_t kPageAccent[3] = { 0x07FF, 0xFD20, 0x8EF1 };

// Returns display string for discrete-select MOD params, nullptr for continuous.
static const char* GetModEnumLabel(ModModeId mode, int param_idx, float norm) {
    if (param_idx != static_cast<int>(mod_fx::ParamId::P2)) return nullptr;  // only P2 is enum in the 6 mod modes
    switch (mode) {
        case ModModeId::Phaser: {
            static const char* k[] = {"2 ST","4 ST","6 ST","8 ST","12 ST","16 ST","BARBER"};
            const int i = static_cast<int>(norm * 6.999f);
            return k[i < 7 ? i : 6];
        }
        case ModModeId::Chorus: {
            static const char* k[] = {"dBUCKET","MULTI","VIBRATO","DETUNE","DIGITAL"};
            const int i = static_cast<int>(norm * 4.999f);
            return k[i < 5 ? i : 4];
        }
        case ModModeId::Flanger: {
            static const char* k[] = {"SILVER","GREY","BLACK+","BLACK-","ZERO+","ZERO-"};
            const int i = static_cast<int>(norm * 5.999f);
            return k[i < 6 ? i : 5];
        }
        case ModModeId::VintTrem: {
            static const char* k[] = {"TUBE","HARMONIC","PHOTO"};
            const int i = static_cast<int>(norm * 2.999f);
            return k[i < 3 ? i : 2];
        }
        default: return nullptr;
    }
}

// ── Init ─────────────────────────────────────────────────────────────────────

void DisplayManager::Init() {
    driver_.Init();
    DisplayRenderer::Clear(kColorBlack);
    driver_.FillScreen(kColorBlack);
}

// ── Update ───────────────────────────────────────────────────────────────────

void DisplayManager::Update(int           active_page,
                            bool          shift,
                            ModModeId     mod_mode,
                            DelayModeId   delay_mode,
                            ReverbModeId  reverb_mode,
                            const mod_fx::ParamSet&    mod_params,
                            const delay_fx::ParamSet&  delay_params,
                            const reverb_fx::ParamSet& reverb_params,
                            const bool    fx_enabled[3],
                            bool          hold_active,
                            int           preset_slot,
                            PresetUiEvent preset_event,
                            uint32_t      now_ms) {
    (void)now_ms;
    if (driver_.IsBusy()) return;

    Render(active_page, shift, mod_mode, delay_mode, reverb_mode,
           mod_params, delay_params, reverb_params,
           fx_enabled, hold_active, preset_slot, preset_event);

    driver_.StartDmaTransfer(DisplayRenderer::FrameBuffer(),
                             DisplayRenderer::FrameBufferBytes(),
                             nullptr, nullptr);
}

// ── Render ───────────────────────────────────────────────────────────────────

void DisplayManager::Render(int           active_page,
                            bool          shift,
                            ModModeId     mod_mode,
                            DelayModeId   delay_mode,
                            ReverbModeId  reverb_mode,
                            const mod_fx::ParamSet&    mod_params,
                            const delay_fx::ParamSet&  delay_params,
                            const reverb_fx::ParamSet& reverb_params,
                            const bool    fx_enabled[3],
                            bool          hold_active,
                            int           preset_slot,
                            PresetUiEvent preset_event) {
    if (active_page < 0 || active_page > 2) return;
    const uint16_t accent = kPageAccent[active_page];
    DisplayRenderer::Clear(kColorBlack);

    // ── Tab strip ──────────────────────────────────────────────────────────────
    static const char* kTabLabels[3] = { "MOD", "DLY", "REV" };
    for (int t = 0; t < 3; ++t) {
        const uint16_t tx = static_cast<uint16_t>(t * layout::TAB_W);
        if (t == active_page) {
            DisplayRenderer::FillRect(tx, 0, layout::TAB_W, layout::TAB_H, kPageAccent[t]);
            DisplayRenderer::DrawText(tx + layout::TAB_TEXT_X_OFF, layout::TAB_TEXT_Y,
                                      kTabLabels[t], kColorBlack, kPageAccent[t], Font_11x18);
        } else {
            DisplayRenderer::DrawText(tx + layout::TAB_TEXT_X_OFF, layout::TAB_TEXT_Y,
                                      kTabLabels[t], kPageAccent[t], kColorBlack, Font_11x18);
        }
    }

    // ── Mode name ──────────────────────────────────────────────────────────────
    const char* mode_name = (active_page == 0) ? ModModeName(mod_mode) :
                            (active_page == 1) ? DelayModeName(delay_mode) :
                                                 ReverbModeName(reverb_mode);
    DisplayRenderer::DrawText(layout::MODE_X, layout::MODE_Y,
                              mode_name, accent, kColorBlack, Font_11x18);

    // Preset slot "P1".."P8"
    char slot_buf[3] = { 'P', static_cast<char>('1' + (preset_slot % 8)), 0 };
    DisplayRenderer::DrawText(layout::SLOT_X, layout::SLOT_Y,
                              slot_buf, kColorWhite, kColorBlack, Font_7x10);

    // ── Separator lines ────────────────────────────────────────────────────────
    DisplayRenderer::HLine(0, layout::SEP1_Y, layout::SCREEN_W, kColorWhite);
    DisplayRenderer::HLine(0, layout::SEP2_Y, layout::SCREEN_W, kColorWhite);
    DisplayRenderer::HLine(0, layout::SEP3_Y, layout::SCREEN_W, kColorWhite);

    // ── Parameter label arrays ─────────────────────────────────────────────────
    static const char* kModLabels[7]   = { "SPEED", "DEPTH", "MIX", "TONE",
                                           "P1",    "P2",    "LEVEL" };
    static const char* kDelayLabels[7] = { "TIME",  "REPEATS", "MIX",
                                           "FILTER","GRIT","MOD SPD","MOD DEP" };
    static const char* kReverbLabels[7]= { "DECAY", "PRE DLY", "MIX",
                                           "TONE",  "MOD", "PARAM1", "PARAM2" };

    // Per-mode overrides for MOD P1/P2 labels
    struct ModAlgoDesc { const char* p1; const char* p2; };
    static const ModAlgoDesc kModAlgoDesc[NUM_MOD_MODES] = {
        {"DELAY",  "TYPE"},    // Chorus
        {"REGEN",  "TYPE"},    // Flanger
        {"DRIVE",  "SPEED"},   // Rotary
        {"REGEN",  "P2"},      // Vibe
        {"REGEN",  "STAGES"},  // Phaser
        {"P1",     "TYPE"},    // VintTrem
    };

    // Apply per-mode overrides for MOD page
    const char* mod_labels[7];
    for (int i = 0; i < 7; ++i) mod_labels[i] = kModLabels[i];
    if (active_page == 0) {
        const int mi = static_cast<int>(mod_mode);
        if (mi >= 0 && mi < NUM_MOD_MODES) {
            mod_labels[4] = kModAlgoDesc[mi].p1;
            mod_labels[5] = kModAlgoDesc[mi].p2;
        }
    }

    // Apply AlgoParamDescriptor overrides for REVERB page
    const char* reverb_labels[7];
    for (int i = 0; i < 7; ++i) reverb_labels[i] = kReverbLabels[i];
    if (active_page == 2) {
        const auto& desc = reverb_fx::get_algo_param_descriptor(reverb_mode);
        if (desc.param1_name[0] != '\0') reverb_labels[5] = desc.param1_name;
        if (desc.param2_name[0] != '\0') reverb_labels[6] = desc.param2_name;
    }

    const char** labels = (active_page == 0) ? mod_labels :
                          (active_page == 1) ? kDelayLabels : reverb_labels;

    // ── Bar values (normalized [0,1]) ──────────────────────────────────────────
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

    // ── 4 parameter rows ───────────────────────────────────────────────────────
    // Primary (shift=false): draw params 0-3
    // Shift   (shift=true):  draw params 4-6 in rows 0-2; row 3 blank
    const int base_idx = shift ? 4 : 0;
    const int count    = shift ? 3 : 4;

    for (int row = 0; row < 4; ++row) {
        if (row >= count) continue;   // row left blank (black background)
        const int param_idx = base_idx + row;
        const uint16_t row_y = static_cast<uint16_t>(
            layout::PARAM_AREA_Y + row * layout::PARAM_ROW_H);

        DisplayRenderer::DrawText(layout::LABEL_X,
                                  row_y + layout::LABEL_OFFSET_Y,
                                  labels[param_idx], kColorWhite, kColorBlack, Font_11x18);

        // Enum or bar?
        const char* enum_str = (active_page == 0)
                                ? GetModEnumLabel(mod_mode, param_idx, bar[param_idx])
                                : nullptr;
        if (enum_str) {
            DisplayRenderer::DrawText(layout::BAR_X,
                                      row_y + layout::BAR_OFFSET_Y,
                                      enum_str, accent, kColorBlack, Font_11x18);
        } else {
            DisplayRenderer::DrawBar(layout::BAR_X,
                                     row_y + layout::BAR_OFFSET_Y,
                                     layout::BAR_W, layout::BAR_H,
                                     bar[param_idx], accent);
        }
    }

    // ── Status row ────────────────────────────────────────────────────────────
    if (hold_active) {
        DisplayRenderer::DrawText(layout::HOLD_X, layout::STATUS_Y,
                                  "HOLD", kColorRed, kColorBlack, Font_6x8);
    }
    if (preset_event == PresetUiEvent::Saved) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "SAVED", kColorWhite, kColorBlack, Font_6x8);
    } else if (preset_event == PresetUiEvent::Loaded) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "LOAD", kColorWhite, kColorBlack, Font_6x8);
    } else if (preset_event == PresetUiEvent::Error) {
        DisplayRenderer::DrawText(layout::PRESET_EVT_X, layout::STATUS_Y,
                                  "ERR", kColorRed, kColorBlack, Font_6x8);
    }

    // ── Chain strip (y=292..319) ──────────────────────────────────────────────
    // Three sections: [MOD: <name>] > [DLY: <name>] > [REV: <name>]
    // Disabled sections render in kColorDim with a strikethrough across the name.
    static const char* kChainTags[3] = { "MOD", "DLY", "REV" };
    const char* chain_names[3] = {
        ModModeName(mod_mode),
        DelayModeName(delay_mode),
        ReverbModeName(reverb_mode),
    };

    for (int s = 0; s < 3; ++s) {
        const uint16_t sx = static_cast<uint16_t>(s * layout::CHAIN_SEC_W
                                                  + layout::CHAIN_INNER_X);
        const bool     en = fx_enabled[s];
        const uint16_t tag_col = en ? kPageAccent[s] : kColorDim;
        const uint16_t name_col = en ? ((active_page == s) ? kPageAccent[s] : kColorWhite)
                                     : kColorDim;

        DisplayRenderer::DrawText(sx, layout::CHAIN_TAG_Y,
                                  kChainTags[s], tag_col, kColorBlack, Font_6x8);
        DisplayRenderer::DrawText(sx, layout::CHAIN_NAME_Y,
                                  chain_names[s], name_col, kColorBlack, Font_6x8);

        if (!en) {
            // Strikethrough centred on the 8px font, ~70px long.
            DisplayRenderer::HLine(sx,
                                   static_cast<uint16_t>(layout::CHAIN_NAME_Y + 4),
                                   72, kColorDim);
        }
    }

    DisplayRenderer::DrawText(layout::CHAIN_ARR_X1, layout::CHAIN_ARR_Y,
                              ">", kColorWhite, kColorBlack, Font_6x8);
    DisplayRenderer::DrawText(layout::CHAIN_ARR_X2, layout::CHAIN_ARR_Y,
                              ">", kColorWhite, kColorBlack, Font_6x8);
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
