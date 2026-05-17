# ── Multi-FX Pedal (reverb + modulation + delay, chained) ────────────────────
TARGET   = multi-fx
#APP_TYPE = BOOT_QSPI

# ── Source files ──────────────────────────────────────────────────────────────
CPP_SOURCES = \
    src/main.cpp \
    src/hardware/controls.cpp \
    src/audio/audio_engine.cpp \
    \
    src/params/delay_param_set.cpp \
    src/params/delay_param_map.cpp \
    src/params/mod_param_set.cpp \
    src/params/mod_param_map.cpp \
    src/params/reverb_param_set.cpp \
    src/params/reverb_param_map.cpp \
    \
    src/dsp/delay_line_sdram.cpp \
    src/dsp/envelope_follower.cpp \
    src/dsp/lfo.cpp \
    src/dsp/tone_filter.cpp \
    src/dsp/fdn.cpp \
    src/dsp/pitch_shifter.cpp \
    \
    src/modes/digital_delay.cpp \
    src/modes/tape_delay.cpp \
    src/modes/dual_delay.cpp \
    src/modes/filter_delay.cpp \
    src/modes/delay_mode_registry.cpp \
    \
    src/modes/chorus_mode.cpp \
    src/modes/flanger_mode.cpp \
    src/modes/rotary_mode.cpp \
    src/modes/vibe_mode.cpp \
    src/modes/phaser_mode.cpp \
    src/modes/vintage_trem_mode.cpp \
    src/modes/mod_mode_registry.cpp \
    \
    src/modes/room_reverb.cpp \
    src/modes/hall_reverb.cpp \
    src/modes/plate_reverb.cpp \
    src/modes/spring_reverb.cpp \
    src/modes/reverb_mode_registry.cpp \
    \
    src/midi/midi_handler.cpp \
    src/display/st7789_driver.cpp \
    src/display/display_renderer.cpp \
    src/display/display_manager.cpp \
    src/presets/preset_manager.cpp \
    src/tempo/tap_tempo.cpp \
    src/tempo/tempo_sync.cpp

# ── Library paths ─────────────────────────────────────────────────────────────
LIBDAISY_DIR = third_party/libDaisy
DAISYSP_DIR  = third_party/DaisySP

# 'src' is added so that all #include "..." paths resolve relative to src/ root
C_INCLUDES += -Isrc

# Size-optimised + LTO: deduplicates inlined param/DSP helpers across 14 mode TUs
OPT = -Os -flto

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# ── QSPI DFU sector padding ───────────────────────────────────────────────────
# dfu-util checks that the sector containing the LAST BYTE of the binary is
# writeable.  The QSPI sector 0x90050000–0x9005FFFF is marked non-writeable by
# the Daisy bootloader DFU descriptor.  Pad to ≥131073 bytes so the last byte
# lands at ≥0x90060000, which is in a writeable sector.
all: pad-qspi-bin

.PHONY: pad-qspi-bin
pad-qspi-bin: $(BUILD_DIR)/$(TARGET).bin
	@sz=$$(wc -c < $<); \
	if [ $$((sz)) -lt 131073 ]; then \
		truncate -s 131073 $<; \
		echo "  PADDED  $$((sz)) -> 131073 bytes (QSPI sector alignment)"; \
	fi
