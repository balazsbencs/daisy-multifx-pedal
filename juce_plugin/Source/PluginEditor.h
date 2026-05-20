#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace pedal {

class PedalLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PedalLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DaisyMultiFxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::Timer
{
public:
    DaisyMultiFxAudioProcessorEditor (DaisyMultiFxAudioProcessor&);
    ~DaisyMultiFxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DaisyMultiFxAudioProcessor& processor;
    PedalLookAndFeel lookAndFeel_;

    // Custom UI Components
    struct EffectSection {
        juce::GroupComponent backgroundCard;
        juce::Label titleLabel;
        juce::ComboBox modeSelector;
        juce::ToggleButton bypassButton; // Footswitch-like stomp button
        juce::Label bypassLabel;
        
        // Dynamic controls
        struct ParameterControl {
            juce::Slider slider;
            juce::Label label;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        };
        std::vector<std::unique_ptr<ParameterControl>> params;

        // Attachment for mode & bypass
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

        // Tempo sync controls
        juce::ToggleButton syncButton;
        juce::ComboBox syncNoteSelector;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncNoteAttachment;
    };

    EffectSection modSection_;
    EffectSection delaySection_;
    EffectSection reverbSection_;

    // Additional controls
    juce::ToggleButton reverbHoldButton;
    juce::Label reverbHoldLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverbHoldAttachment;

    // Signal chain connectors & status indicators
    void drawSignalChain(juce::Graphics& g);

    // Track active modes to update parameter labels dynamically
    int lastModMode_ = -1;
    int lastDelayMode_ = -1;
    int lastReverbMode_ = -1;
    bool lastDelaySync_ = false;
    bool lastModSync_ = false;
    bool lastModBypassed_ = false;
    bool lastDelayBypassed_ = false;
    bool lastReverbBypassed_ = false;

    bool updateDynamicUi();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaisyMultiFxAudioProcessorEditor)
};

} // namespace pedal
