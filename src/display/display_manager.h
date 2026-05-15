#pragma once
#include "st7789_driver.h"
#include "../config/constants.h"
#include "../config/delay_mode_id.h"
#include "../config/mod_mode_id.h"
#include "../config/reverb_mode_id.h"
#include "../params/delay_param_set.h"
#include "../params/mod_param_set.h"
#include "../params/reverb_param_set.h"
#include <cstdint>

namespace pedal {

enum class PresetUiEvent { None, Saved, Loaded, Error };

class DisplayManager {
public:
    void Init();

    void Update(int           active_page,
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
                uint32_t      now_ms);

private:
    St7789Driver driver_;

    void Render(int           active_page,
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
                PresetUiEvent preset_event);

    static const char* ModModeName(ModModeId id);
    static const char* DelayModeName(DelayModeId id);
    static const char* ReverbModeName(ReverbModeId id);
};

} // namespace pedal
