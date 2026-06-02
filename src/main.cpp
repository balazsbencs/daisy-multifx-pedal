#include "daisy_seed.h"
#include "config/constants.h"
#include "config/delay_mode_id.h"
#include "config/mod_mode_id.h"
#include "config/reverb_mode_id.h"
#include "config/pin_map.h"
#include "hardware/controls.h"
#include "audio/audio_engine.h"
#include "modes/delay_mode_registry.h"
#include "modes/mod_mode_registry.h"
#include "modes/reverb_mode_registry.h"
#include "params/delay_param_map.h"
#include "params/mod_param_map.h"
#include "params/reverb_param_map.h"
#include "params/param_range.h"
#include "midi/midi_handler.h"
#include "display/display_manager.h"
#include "tempo/tempo_sync.h"
#include "presets/qspi_preset_store.h"

using namespace daisy;
using namespace pedal;

// ── Hardware ─────────────────────────────────────────────────────────────────
static DaisySeed hw;

// ── Subsystems ────────────────────────────────────────────────────────────────
static AudioEngine       audio_engine;
static Controls          controls;
static DelayModeRegistry delay_registry;
static ModModeRegistry   mod_registry;
static ReverbModeRegistry reverb_registry;
static MidiHandlerPedal  midi_handler;
static DisplayManager    display;
static TempoSync         tempo_sync;
static QspiPresetStore preset_store;

// ── Main-loop state ───────────────────────────────────────────────────────────
static int active_page = 0;  // 0 = mod, 1 = delay, 2 = reverb

static float     mod_norm[NUM_PARAMS]    = {0.3f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f};
static float     delay_norm[NUM_PARAMS]  = {0.5f, 0.4f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f};
static float     reverb_norm[NUM_PARAMS] = {0.4f, 0.04f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f};

static ModModeId    cur_mod    = ModModeId::Chorus;
static DelayModeId  cur_delay  = DelayModeId::Tape;
static ReverbModeId cur_reverb = ReverbModeId::Hall;

static bool hold_active   = false;
static int  preset_bank   = 0;
static int  preset_slot   = 0;
static bool fx_enabled[3] = {true, false, false}; // 0=mod,1=delay,2=reverb — off by default

static bool     live_state_dirty            = false;
static uint32_t last_change_ms              = 0;
constexpr uint32_t kLiveStateSaveDebounceMs = 2000;

// Preset browse mode
enum class PresetMode { Normal, Browse };
static PresetMode preset_mode         = PresetMode::Normal;
static int        browse_bank         = 0;
static int        browse_slot         = 0;
static bool       tap_preset_pending  = false;
static uint32_t   tap_hold_start_ms   = 0;
static uint32_t   last_browse_ms      = 0;
static constexpr uint32_t kPresetEntryHoldMs      = 1000u;
static constexpr uint32_t kPresetInactivityMs     = 3000u;
static constexpr uint32_t kPresetLedBlinkPeriodMs = 500u;

// Status LEDs (one per effect footswitch)
static daisy::GPIO led_fx[3];

static uint32_t last_enc_tick_ms[NUM_PARAMS]{};

// ── Helpers ───────────────────────────────────────────────────────────────────
static float Clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static int WrapIndex(int value, int count) {
    value %= count;
    return (value < 0) ? (value + count) : value;
}

static MultiPresetSlot SnapshotLiveState() {
    MultiPresetSlot s{};
    s.valid       = 1;
    s.mod_mode    = static_cast<uint8_t>(cur_mod);
    s.delay_mode  = static_cast<uint8_t>(cur_delay);
    s.reverb_mode = static_cast<uint8_t>(cur_reverb);
    for (int i = 0; i < NUM_PARAMS; ++i) {
        s.mod_norm[i]    = mod_norm[i];
        s.delay_norm[i]  = delay_norm[i];
        s.reverb_norm[i] = reverb_norm[i];
    }
    for (int i = 0; i < 3; ++i) s.fx_enabled[i] = fx_enabled[i] ? 1 : 0;
    return s;
}

static void ApplyEncoderEdit(float& target, int delta, uint32_t now,
                              uint32_t& last_tick) {
    if (!delta) return;
    const int   dir   = delta > 0 ? 1 : -1;
    int         steps = delta > 0 ? delta : -delta;
    while (steps--) {
        const bool  fast = last_tick && (now - last_tick <= ENCODER_FAST_WINDOW_MS);
        const float step = fast ? PARAM_STEP_FAST : PARAM_STEP_SLOW;
        target           = Clamp01(target + dir * step);
        last_tick        = now;
    }
}

static delay_fx::ParamSet BuildDelayParams(float tempo_override) {
    using namespace delay_fx;
    ParamSet ps;
    ps.time    = map_param(delay_norm[0], get_param_range(cur_delay, ParamId::Time));
    ps.repeats = map_param(delay_norm[1], get_param_range(cur_delay, ParamId::Repeats));
    ps.mix     = map_param(delay_norm[2], get_param_range(cur_delay, ParamId::Mix));
    ps.filter  = map_param(delay_norm[3], get_param_range(cur_delay, ParamId::Filter));
    ps.grit    = map_param(delay_norm[4], get_param_range(cur_delay, ParamId::Grit));
    ps.mod_spd = map_param(delay_norm[5], get_param_range(cur_delay, ParamId::ModSpd));
    ps.mod_dep = map_param(delay_norm[6], get_param_range(cur_delay, ParamId::ModDep));
    if (tempo_override > 0.0f) ps.time = tempo_override;
    return ps;
}

static mod_fx::ParamSet BuildModParams(float tempo_hz_override) {
    using namespace mod_fx;
    ParamSet ps;
    ps.speed = map_param(mod_norm[0], get_param_range(cur_mod, ParamId::Speed));
    ps.depth = map_param(mod_norm[1], get_param_range(cur_mod, ParamId::Depth));
    ps.mix   = map_param(mod_norm[2], get_param_range(cur_mod, ParamId::Mix));
    ps.tone  = map_param(mod_norm[3], get_param_range(cur_mod, ParamId::Tone));
    ps.p1    = map_param(mod_norm[4], get_param_range(cur_mod, ParamId::P1));
    ps.p2    = map_param(mod_norm[5], get_param_range(cur_mod, ParamId::P2));
    ps.level = map_param(mod_norm[6], get_param_range(cur_mod, ParamId::Level));
    if (tempo_hz_override > 0.0f) ps.speed = tempo_hz_override;
    return ps;
}

static reverb_fx::ParamSet BuildReverbParams() {
    using namespace reverb_fx;
    ParamSet ps;
    ps.decay     = map_param(reverb_norm[0], get_param_range(cur_reverb, ParamId::Decay));
    ps.pre_delay = map_param(reverb_norm[1], get_param_range(cur_reverb, ParamId::PreDelay));
    ps.mix       = map_param(reverb_norm[2], get_param_range(cur_reverb, ParamId::Mix));
    ps.tone      = map_param(reverb_norm[3], get_param_range(cur_reverb, ParamId::Tone));
    ps.mod       = map_param(reverb_norm[4], get_param_range(cur_reverb, ParamId::Mod));
    ps.param1    = map_param(reverb_norm[5], get_param_range(cur_reverb, ParamId::Param1));
    ps.param2    = map_param(reverb_norm[6], get_param_range(cur_reverb, ParamId::Param2));
    return ps;
}

// ── Mode switching ────────────────────────────────────────────────────────────
static void SwitchDelayMode(DelayModeId id) {
    cur_delay = id;
    delay_registry.Reset(id);
    audio_engine.SetDelayMode(delay_registry.get(id));
}

static void SwitchModMode(ModModeId id) {
    cur_mod = id;
    mod_registry.Reset(id);
    audio_engine.SetModMode(mod_registry.get(id));
}

static void SwitchReverbMode(ReverbModeId id) {
    cur_reverb = id;
    reverb_registry.Reset(id);
    audio_engine.SetReverbMode(reverb_registry.get(id));
}

static void LoadPreset(int bank, int slot) {
    MultiPresetSlot p{};
    if (!preset_store.LoadSlot(bank, slot, p)) return;
    if (p.mod_mode    < static_cast<uint8_t>(NUM_MOD_MODES))    SwitchModMode(static_cast<ModModeId>(p.mod_mode));
    if (p.delay_mode  < static_cast<uint8_t>(NUM_DELAY_MODES))  SwitchDelayMode(static_cast<DelayModeId>(p.delay_mode));
    if (p.reverb_mode < static_cast<uint8_t>(NUM_REVERB_MODES)) SwitchReverbMode(static_cast<ReverbModeId>(p.reverb_mode));
    for (int i = 0; i < NUM_PARAMS; ++i) {
        mod_norm[i]    = Clamp01(p.mod_norm[i]);
        delay_norm[i]  = Clamp01(p.delay_norm[i]);
        reverb_norm[i] = Clamp01(p.reverb_norm[i]);
    }
    for (int i = 0; i < 3; ++i) {
        fx_enabled[i] = p.fx_enabled[i];
        led_fx[i].Write(fx_enabled[i]);
    }
    preset_bank = bank;
    preset_slot = slot;
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main() {
    hw.Init();
    hw.SetAudioBlockSize(BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    controls.Init(hw);
    delay_registry.Init();
    mod_registry.Init();
    reverb_registry.Init();

    led_fx[0].Init(pins::LED_FX_MOD,    GPIO::Mode::OUTPUT);
    led_fx[1].Init(pins::LED_FX_DELAY,  GPIO::Mode::OUTPUT);
    led_fx[2].Init(pins::LED_FX_REVERB, GPIO::Mode::OUTPUT);

    // ── Boot diagnostics: LED pattern encodes last completed checkpoint ──────
    // If the device freezes during init the LEDs hold their last-written state.
    // Pattern key (MOD / DELAY / REVERB):
    //   1,0,0 = after LED GPIO init
    //   0,1,0 = after audio_engine + mode switches
    //   1,1,0 = after midi_handler.Init
    //   0,0,1 = after display.Init
    //   1,0,1 = after preset_store.Init
    //   0,1,1 = after live-state restore
    //   1,1,1 = just before StartAudio  (main loop will overwrite these)
    auto diag = [&](bool a, bool b, bool c) {
        led_fx[0].Write(a); led_fx[1].Write(b); led_fx[2].Write(c);
    };

    diag(1,0,0);
    audio_engine.Init(&hw);
    SwitchModMode(cur_mod);
    SwitchDelayMode(cur_delay);
    SwitchReverbMode(cur_reverb);

    diag(0,1,0);
    midi_handler.Init(hw, preset_store);
    // libDaisy's default UART MIDI uses D13/D14, which this build also uses
    // for the ST7789 CS/DC pins. Init the display after MIDI so those pins
    // end up configured for the screen, matching the working delay project.
    diag(1,1,0);
    display.Init();
    diag(0,0,1);
    preset_store.Init(hw.qspi);
    diag(1,0,1);

    // ── Restore live state on boot ─────────────────────────────────────────
    {
        MultiPresetSlot live;
        if (preset_store.LoadLiveState(live) &&
            live.mod_mode    < static_cast<uint8_t>(NUM_MOD_MODES) &&
            live.delay_mode  < static_cast<uint8_t>(NUM_DELAY_MODES) &&
            live.reverb_mode < static_cast<uint8_t>(NUM_REVERB_MODES)) {
            SwitchModMode(static_cast<ModModeId>(live.mod_mode));
            SwitchDelayMode(static_cast<DelayModeId>(live.delay_mode));
            SwitchReverbMode(static_cast<ReverbModeId>(live.reverb_mode));
            for (int i = 0; i < NUM_PARAMS; ++i) {
                mod_norm[i]    = Clamp01(live.mod_norm[i]);
                delay_norm[i]  = Clamp01(live.delay_norm[i]);
                reverb_norm[i] = Clamp01(live.reverb_norm[i]);
            }
            for (int i = 0; i < 3; ++i) fx_enabled[i] = live.fx_enabled[i];
        }
    }
    diag(0,1,1);

    tempo_sync.Init();

    diag(1,1,1);
    hw.StartAudio(AudioEngine::AudioCallback);
    for (int i = 0; i < 3; ++i) led_fx[i].Write(fx_enabled[i]);

    static uint32_t display_last_ms  = 0;
    static bool     mode_hold_consumed  = false;
    static bool     mode_hold_active    = false;
    static uint32_t mode_hold_start_ms  = 0;
    constexpr uint32_t kModeHoldThreshMs = 250;

    while (true) {
        const uint32_t now = System::GetNow();
        controls.Poll();
        const ControlState& ctrl = controls.state();

        // ── Effect footswitches (normal) / browse nav (preset mode) ────────────
        if (preset_mode == PresetMode::Normal) {
            for (int i = 0; i < 3; ++i) {
                if (ctrl.fx_pressed[i]) {
                    fx_enabled[i]    = !fx_enabled[i];
                    led_fx[i].Write(fx_enabled[i]);
                    live_state_dirty = true;
                    last_change_ms   = now;
                }
            }
        } else { // PresetMode::Browse
            // fx[0]=MOD → prev, fx[1]=DELAY → save, fx[2]=REVERB → next
            if (ctrl.fx_pressed[0]) {
                if (--browse_slot < 0) {
                    browse_slot = PRESET_SLOTS_PER_BANK - 1;
                    browse_bank = (browse_bank - 1 + PRESET_BANK_COUNT) % PRESET_BANK_COUNT;
                }
                LoadPreset(browse_bank, browse_slot);
                last_browse_ms = now;
            }
            if (ctrl.fx_pressed[2]) {
                if (++browse_slot >= PRESET_SLOTS_PER_BANK) {
                    browse_slot = 0;
                    browse_bank = (browse_bank + 1) % PRESET_BANK_COUNT;
                }
                LoadPreset(browse_bank, browse_slot);
                last_browse_ms = now;
            }
            if (ctrl.fx_pressed[1]) {
                const MultiPresetSlot snap = SnapshotLiveState();
                preset_store.SaveSlot(browse_bank, browse_slot, snap);
                last_browse_ms = now;
            }

            // Blink all three LEDs at 2 Hz.
            const bool blink_on = ((now % kPresetLedBlinkPeriodMs) < (kPresetLedBlinkPeriodMs / 2u));
            for (int i = 0; i < 3; ++i) led_fx[i].Write(blink_on);

            // Auto-exit after inactivity.
            if ((now - last_browse_ms) >= kPresetInactivityMs) {
                LoadPreset(preset_store.GetActiveBank(), preset_store.GetActiveSlot());
                preset_mode = PresetMode::Normal;
                for (int i = 0; i < 3; ++i) led_fx[i].Write(fx_enabled[i]);
            }
        }

        // ── Mode encoder hold tracking ───────────────────────────────────────
        if (ctrl.mode_encoder_held && !mode_hold_active) {
            mode_hold_start_ms = now;
            mode_hold_active   = true;
        }
        if (!ctrl.mode_encoder_held) {
            mode_hold_active = false;
        }

        // ── Page switching (mode encoder button) ─────────────────────────────
        if (ctrl.mode_encoder_pressed) {
            const bool long_hold = (now - mode_hold_start_ms) >= kModeHoldThreshMs;
            if (!mode_hold_consumed && !long_hold) {
                active_page = (active_page + 1) % 3;
            }
            mode_hold_consumed = false;
        }

        // ── Mode selection within active page (mode encoder rotation) ─────────
        const int mode_delta = ctrl.mode_encoder_increment;
        if (mode_delta && !ctrl.mode_encoder_held) {
            const int dir = (mode_delta > 0) ? 1 : -1;
            switch (active_page) {
                case 0: {
                    const int m = WrapIndex(static_cast<int>(cur_mod) + dir,
                                            NUM_MOD_MODES);
                    SwitchModMode(static_cast<ModModeId>(m));
                    break;
                }
                case 1: {
                    const int m = WrapIndex(static_cast<int>(cur_delay) + dir,
                                            NUM_DELAY_MODES);
                    SwitchDelayMode(static_cast<DelayModeId>(m));
                    break;
                }
                case 2: {
                    const int m = WrapIndex(static_cast<int>(cur_reverb) + dir,
                                            NUM_REVERB_MODES);
                    SwitchReverbMode(static_cast<ReverbModeId>(m));
                    break;
                }
            }
            live_state_dirty = true;
            last_change_ms   = now;
        }

        // ── Parameter encoders → active page params ────────────────────────────
        float* active_norm = (active_page == 0) ? mod_norm :
                             (active_page == 1) ? delay_norm : reverb_norm;

        const bool shift = ctrl.mode_encoder_held;
        for (int p = 0; p < 4; ++p) {
            const int delta = ctrl.param_encoder_increment[p];
            if (!delta) continue;
            if (shift) {
                mode_hold_consumed = true;
            }
            const int param_idx = shift ? (p + 4) : p;
            if (param_idx < NUM_PARAMS) {
                ApplyEncoderEdit(active_norm[param_idx], delta, now,
                                 last_enc_tick_ms[param_idx]);
                live_state_dirty = true;
                last_change_ms   = now;
            }
        }

        // ── Tap footswitch ─────────────────────────────────────────────────────
        if (preset_mode == PresetMode::Normal) {
            if (ctrl.tap_pressed) {
                tap_hold_start_ms  = now;
                tap_preset_pending = false;
                tempo_sync.OnTap(now);
            }
            if (ctrl.tap_held && !tap_preset_pending &&
                (now - tap_hold_start_ms) >= kPresetEntryHoldMs) {
                tap_preset_pending = true;
                if (hold_active) { hold_active = false; audio_engine.SetHold(false); }
            }
            if (!tap_preset_pending && ctrl.tap_held &&
                ctrl.tap_held_ms > 500 && !hold_active) {
                hold_active = true;
                audio_engine.SetHold(true);
            }
            if (ctrl.tap_released) {
                if (tap_preset_pending) {
                    browse_bank        = preset_bank;
                    browse_slot        = preset_slot;
                    preset_mode        = PresetMode::Browse;
                    last_browse_ms     = now;
                    tap_preset_pending = false;
                } else if (hold_active && ctrl.tap_held_ms <= 500) {
                    hold_active = false;
                    audio_engine.SetHold(false);
                }
            }
        } else { // PresetMode::Browse
            if (ctrl.tap_pressed) {
                preset_store.SetActive(browse_bank, browse_slot);
                preset_bank = browse_bank;
                preset_slot = browse_slot;
                preset_mode = PresetMode::Normal;
                for (int i = 0; i < 3; ++i) led_fx[i].Write(fx_enabled[i]);
            }
        }

        // ── MIDI ───────────────────────────────────────────────────────────────
        MultiMidiState midi;
        midi_handler.Poll(midi);

        for (int p = 0; p < NUM_PARAMS; ++p) {
            if (midi.mod_cc_rx[p])    { mod_norm[p]    = midi.mod_cc[p];    live_state_dirty = true; last_change_ms = now; }
            if (midi.delay_cc_rx[p])  { delay_norm[p]  = midi.delay_cc[p];  live_state_dirty = true; last_change_ms = now; }
            if (midi.reverb_cc_rx[p]) { reverb_norm[p] = midi.reverb_cc[p]; live_state_dirty = true; last_change_ms = now; }
        }
        if (midi.hold_on)  { hold_active = true;  audio_engine.SetHold(true);  }
        if (midi.hold_off) { hold_active = false; audio_engine.SetHold(false); }

        if (midi.clock_tick) tempo_sync.OnMidiClock(now);
        if (midi.clock_stop) tempo_sync.OnMidiStop();

        if (midi.preset_load) {
            LoadPreset(midi.sysex_bank, midi.sysex_slot);
            live_state_dirty = true;
            last_change_ms   = now;
        }
        if (midi.mode_change) {
            const int idx = midi.mode_index;
            switch (midi.mode_stage) {
                case 0: if (idx >= 0 && idx < NUM_MOD_MODES)    SwitchModMode(static_cast<ModModeId>(idx));    break;
                case 1: if (idx >= 0 && idx < NUM_DELAY_MODES)  SwitchDelayMode(static_cast<DelayModeId>(idx));  break;
                case 2: if (idx >= 0 && idx < NUM_REVERB_MODES) SwitchReverbMode(static_cast<ReverbModeId>(idx)); break;
            }
            live_state_dirty = true;
            last_change_ms   = now;
        }
        if (midi.fx_enable_change) {
            const int stage = midi.fx_enable_stage;
            if (stage >= 0 && stage < 3) {
                fx_enabled[stage] = midi.fx_enable_val;
                led_fx[stage].Write(fx_enabled[stage]);
            }
            live_state_dirty = true;
            last_change_ms   = now;
        }

        // ── Tempo sync ─────────────────────────────────────────────────────────
        tempo_sync.Process(now);
        const float tempo_s  = tempo_sync.GetOverrideSeconds();
        const float tempo_hz = (tempo_s > 0.0f) ? (1.0f / tempo_s) : 0.0f;

        // ── Build and publish params ───────────────────────────────────────────
        MultiParamBuf buf;
        buf.mod         = BuildModParams(tempo_hz);
        buf.delay       = BuildDelayParams(tempo_s);
        buf.reverb      = BuildReverbParams();
        buf.hold_active = hold_active;
        for (int i = 0; i < 3; ++i) buf.fx_enabled[i] = fx_enabled[i];
        audio_engine.SetParams(buf);

        // ── Display ────────────────────────────────────────────────────────────
        if (now - display_last_ms >= DISPLAY_UPDATE_MS) {
            display_last_ms = now;
            display.Update(active_page, shift,
                           cur_mod, cur_delay, cur_reverb,
                           buf.mod, buf.delay, buf.reverb,
                           fx_enabled, hold_active,
                           preset_bank * PRESET_SLOTS_PER_BANK + preset_slot,
                           PresetUiEvent::None, now);
        }

        // ── Live state auto-save (debounced) ──────────────────────────────────
        if (live_state_dirty && (now - last_change_ms) >= kLiveStateSaveDebounceMs) {
            preset_store.SaveLiveState(SnapshotLiveState());
            live_state_dirty = false;
        }
    }
}
