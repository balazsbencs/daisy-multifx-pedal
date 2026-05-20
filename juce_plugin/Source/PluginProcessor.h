#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// Include original DSP registries
#include "modes/mod_mode_registry.h"
#include "modes/delay_mode_registry.h"
#include "modes/reverb_mode_registry.h"

namespace pedal {

class DaisyMultiFxAudioProcessor : public juce::AudioProcessor {
public:
    DaisyMultiFxAudioProcessor();
    ~DaisyMultiFxAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS for parameter management
    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // registries
    ModModeRegistry mod_registry_;
    DelayModeRegistry delay_registry_;
    ReverbModeRegistry reverb_registry_;

    // active modes
    ModMode* active_mod_ = nullptr;
    DelayMode* active_delay_ = nullptr;
    ReverbMode* active_reverb_ = nullptr;

    // Sample counter for the 48-sample block rate
    int sampleCounter_ = 0;

    // Replay buffers for coefficient calculations
    mod_fx::ParamSet mod_params_;
    delay_fx::ParamSet delay_params_;
    reverb_fx::ParamSet reverb_params_;

    // Dry/wet coefficients cached per stage
    float mod_dry_ = 1.0f, mod_wet_ = 0.0f, mod_norm_ = 1.0f;
    float dly_dry_ = 1.0f, dly_wet_ = 0.0f, dly_norm_ = 1.0f;
    float rev_dry_ = 1.0f, rev_wet_ = 0.0f, rev_norm_ = 1.0f;

    float last_mod_mix_ = -1.0f;
    float last_dly_mix_ = -1.0f;
    float last_rev_mix_ = -1.0f;

    void updateMixCoeffs(float modMix, float dlyMix, float revMix);

    // Sample rate tracker
    double dawSampleRate_ = 48000.0;

    // Tempo sync helpers
    double currentBpm_ = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DaisyMultiFxAudioProcessor)
};

} // namespace pedal
