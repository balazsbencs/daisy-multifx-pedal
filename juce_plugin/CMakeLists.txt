cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "OSX Deployment Target" FORCE)

project(DaisyMultiFx VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch JUCE 8.0.12 with shallow clone for speed
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

# In CI, the system VST3/AU directories are not writable — skip the copy.
if(DEFINED ENV{CI})
    set(JUCE_COPY_PLUGIN FALSE)
else()
    set(JUCE_COPY_PLUGIN TRUE)
endif()

juce_add_plugin(DaisyMultiFx
    VERSION "1.0.0"
    COMPANY_NAME "Daisy"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${JUCE_COPY_PLUGIN}
    PLUGIN_NAME "DaisyMultiFx"
    PRODUCT_NAME "DaisyMultiFx"
    FORMATS VST3 AU Standalone
)

target_sources(DaisyMultiFx PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # Original DSP files
    ../src/dsp/delay_line_sdram.cpp
    ../src/dsp/envelope_follower.cpp
    ../src/dsp/lfo.cpp
    ../src/dsp/tone_filter.cpp
    ../src/dsp/fdn.cpp
    ../src/dsp/pitch_shifter.cpp

    ../src/params/delay_param_set.cpp
    ../src/params/delay_param_map.cpp
    ../src/params/mod_param_set.cpp
    ../src/params/mod_param_map.cpp
    ../src/params/reverb_param_set.cpp
    ../src/params/reverb_param_map.cpp

    ../src/modes/digital_delay.cpp
    ../src/modes/tape_delay.cpp
    ../src/modes/dual_delay.cpp
    ../src/modes/filter_delay.cpp
    ../src/modes/lofi_delay.cpp
    ../src/modes/dbucket_delay.cpp
    ../src/modes/duck_delay.cpp
    ../src/modes/pattern_delay.cpp
    ../src/modes/swell_delay.cpp
    ../src/modes/trem_delay.cpp
    ../src/modes/delay_mode_registry.cpp

    ../src/modes/chorus_mode.cpp
    ../src/modes/flanger_mode.cpp
    ../src/modes/rotary_mode.cpp
    ../src/modes/vibe_mode.cpp
    ../src/modes/phaser_mode.cpp
    ../src/modes/vintage_trem_mode.cpp
    ../src/modes/poly_octave_mode.cpp
    ../src/modes/mod_mode_registry.cpp

    ../src/modes/room_reverb.cpp
    ../src/modes/hall_reverb.cpp
    ../src/modes/plate_reverb.cpp
    ../src/modes/spring_reverb.cpp
    ../src/modes/bloom_reverb.cpp
    ../src/modes/cloud_reverb.cpp
    ../src/modes/shimmer_reverb.cpp
    ../src/modes/chorale_reverb.cpp
    ../src/modes/nonlinear_reverb.cpp
    ../src/modes/swell_reverb.cpp
    ../src/modes/magneto_reverb.cpp
    ../src/modes/reflections_reverb.cpp
    ../src/modes/reverb_mode_registry.cpp
)

target_include_directories(DaisyMultiFx PRIVATE
    Source
    ../src
    ../third_party/q/q_lib/include
    ../third_party/q/infra/include
)

target_compile_definitions(DaisyMultiFx PRIVATE
    JUCE_SHARED_CODE_IN_PLUGIN_PROJECT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DaisyMultiFx PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_events
    juce::juce_core
)

juce_generate_juce_header(DaisyMultiFx)
