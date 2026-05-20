#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/param_range.h"
#include "params/mod_param_map.h"
#include "params/delay_param_map.h"
#include "params/reverb_param_map.h"

namespace pedal {

// ── LookAndFeel Implementation ───────────────────────────────────────────────

PedalLookAndFeel::PedalLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(20, 20, 22));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(30, 30, 35));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(60, 60, 65));
    setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(100, 100, 105));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(25, 25, 30));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(50, 50, 60));
}

void PedalLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider& slider) {
    auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
    auto fill    = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Read custom accent color property
    juce::Colour accent = juce::Colour::fromString(slider.getProperties().getWithDefault("accentColor", "0xff00ffff").toString());

    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(6.0f);
    auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 3.5f;
    auto arcRadius = radius - lineW * 1.5f;

    auto center = bounds.getCentre();

    // 1. Draw Knob body (3D-ish feel)
    juce::Path knobBody;
    knobBody.addCentredArc(center.x, center.y, radius - 2.0f, radius - 2.0f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
    
    // Gradient for metallic look
    juce::ColourGradient bodyGrad(juce::Colour(45, 45, 50), center.x, center.y - radius,
                                 juce::Colour(25, 25, 28), center.x, center.y + radius, false);
    g.setGradientFill(bodyGrad);
    g.fillPath(knobBody);

    // Bevel ring
    g.setColour(juce::Colour(65, 65, 70));
    g.strokePath(knobBody, juce::PathStrokeType(1.0f));

    // 2. Draw Arc Track (background)
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(15, 15, 18));
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Draw Active Value Arc (glowing)
    if (sliderPos > 0.0f) {
        juce::Path valueArc;
        valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
        
        // Glow effect
        g.setColour(accent.withAlpha(0.2f));
        g.strokePath(valueArc, juce::PathStrokeType(lineW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        g.setColour(accent);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 4. Draw Needle indicator
    juce::Path needle;
    auto needleLen = radius - 7.0f;
    needle.startNewSubPath(center.x, center.y);
    needle.lineTo(center.x + needleLen * std::sin(toAngle), center.y - needleLen * std::cos(toAngle));
    g.setColour(accent.brighter(0.2f));
    g.strokePath(needle, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}

void PedalLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                  int buttonX, int buttonY, int buttonW, int buttonH,
                                  juce::ComboBox& box) {
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
    
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    
    g.setColour(juce::Colour(28, 28, 33));
    g.fillRoundedRectangle(bounds, 4.0f);

    juce::Colour outlineColor = juce::Colour::fromString(box.getProperties().getWithDefault("accentColor", "0xff888888").toString()).withAlpha(0.4f);
    g.setColour(outlineColor);
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

    // Draw little arrow
    juce::Path arrow;
    float arrowW = 8.0f;
    float arrowH = 5.0f;
    float centerX = width - 15.0f;
    float centerY = height * 0.5f;

    arrow.startNewSubPath(centerX - arrowW * 0.5f, centerY - arrowH * 0.5f);
    arrow.lineTo(centerX + arrowW * 0.5f, centerY - arrowH * 0.5f);
    arrow.lineTo(centerX, centerY + arrowH * 0.5f);
    arrow.closeSubPath();

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.fillPath(arrow);
}

void PedalLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                      bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    
    // Check if this is a footswitch styled button (metallic stompbox switch)
    if (button.getProperties().contains("isBypassButton")) {
        auto center = bounds.getCentre();
        float diameter = 44.0f;
        auto buttonRect = juce::Rectangle<float>(center.x - diameter * 0.5f, center.y - diameter * 0.5f + 10.0f, diameter, diameter);

        // Draw shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(buttonRect.translated(0.0f, 3.0f));

        // Stomp metal body (bevels)
        juce::ColourGradient grad1(juce::Colour(120, 120, 125), buttonRect.getX(), buttonRect.getY(),
                                   juce::Colour(70, 70, 75), buttonRect.getRight(), buttonRect.getBottom(), true);
        g.setGradientFill(grad1);
        g.fillEllipse(buttonRect);
        
        g.setColour(juce::Colour(140, 140, 145));
        g.drawEllipse(buttonRect, 1.5f);

        auto innerRect = buttonRect.reduced(6.0f);
        juce::ColourGradient grad2(juce::Colour(180, 180, 185), innerRect.getX(), innerRect.getY(),
                                   juce::Colour(90, 90, 95), innerRect.getRight(), innerRect.getBottom(), true);
        g.setGradientFill(grad2);
        g.fillEllipse(innerRect);

        g.setColour(juce::Colour(210, 210, 215));
        g.drawEllipse(innerRect, 1.0f);

        // Center nut
        auto centerNut = innerRect.reduced(8.0f);
        g.setColour(juce::Colour(50, 50, 52));
        g.fillEllipse(centerNut);

        // LED Indicator (above the switch)
        float ledRadius = 5.0f;
        float ledY = center.y - 25.0f;
        auto ledRect = juce::Rectangle<float>(center.x - ledRadius, ledY - ledRadius, ledRadius * 2, ledRadius * 2);

        bool isBypassed = button.getToggleState(); // true = bypassed = OFF
        bool isActive = !isBypassed;
        juce::Colour ledColor = juce::Colour::fromString(button.getProperties().getWithDefault("accentColor", "0xff00ffff").toString());

        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(ledRect.expanded(1.5f), 1.0f);

        if (isActive) {
            // Glowing LED
            juce::ColourGradient ledGrad(ledColor.brighter(0.5f), center.x, ledY,
                                       ledColor.darker(0.3f), center.x + ledRadius, ledY + ledRadius, true);
            g.setGradientFill(ledGrad);
            g.fillEllipse(ledRect);
            
            // LED Glow halo
            g.setColour(ledColor.withAlpha(0.25f));
            g.fillEllipse(ledRect.expanded(5.0f));
        } else {
            // Dark LED
            g.setColour(juce::Colour(45, 20, 20)); // Dim red/dark color
            g.fillEllipse(ledRect);
        }
        return;
    }

    // Default JUCE styled checkbox for Sync / Note Divs
    float size = 16.0f;
    float y = (bounds.getHeight() - size) * 0.5f;
    auto checkboxRect = juce::Rectangle<float>(2.0f, y, size, size);

    g.setColour(juce::Colour(25, 25, 30));
    g.fillRoundedRectangle(checkboxRect, 3.0f);

    juce::Colour accent = juce::Colour::fromString(button.getProperties().getWithDefault("accentColor", "0xff00ffff").toString());
    g.setColour(accent.withAlpha(0.5f));
    g.drawRoundedRectangle(checkboxRect, 3.0f, 1.0f);

    if (button.getToggleState()) {
        g.setColour(accent);
        juce::Path tick;
        tick.startNewSubPath(checkboxRect.getX() + 3.0f, checkboxRect.getCentreY());
        tick.lineTo(checkboxRect.getCentreX() - 1.0f, checkboxRect.getBottom() - 4.0f);
        tick.lineTo(checkboxRect.getRight() - 3.0f, checkboxRect.getY() + 3.0f);
        g.strokePath(tick, juce::PathStrokeType(2.0f));
    }

    // Draw text label
    g.setFont(13.0f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawText(button.getButtonText(), bounds.withTrimmedLeft(size + 8.0f), juce::Justification::centredLeft);
}

// ── Editor Implementation ────────────────────────────────────────────────────

DaisyMultiFxAudioProcessorEditor::DaisyMultiFxAudioProcessorEditor (DaisyMultiFxAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel(&lookAndFeel_);

    // Setup color palettes
    juce::Colour modColor(0, 229, 255);    // Cyan
    juce::Colour delayColor(255, 145, 0);  // Orange
    juce::Colour reverbColor(213, 0, 249); // Pink/Purple

    auto setupSection = [this](EffectSection& section, const juce::String& title,
                              juce::Colour color, const juce::StringArray& modes,
                              const juce::String& bypassParamId, const juce::String& modeParamId,
                              const std::vector<std::pair<juce::String, juce::String>>& paramIdsAndLabels,
                              bool hasSync, const juce::String& syncParamId, const juce::String& noteParamId)
    {
        addChildComponent(section.backgroundCard);
        
        section.titleLabel.setText(title, juce::dontSendNotification);
        section.titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        section.titleLabel.setJustificationType(juce::Justification::centred);
        section.titleLabel.setColour(juce::Label::textColourId, color.brighter(0.2f));
        addAndMakeVisible(section.titleLabel);

        // Mode selector
        section.modeSelector.addItemList(modes, 1);
        section.modeSelector.getProperties().set("accentColor", color.toString());
        addAndMakeVisible(section.modeSelector);
        section.modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, modeParamId, section.modeSelector);

        // Bypass button
        section.bypassButton.getProperties().set("isBypassButton", true);
        section.bypassButton.getProperties().set("accentColor", color.toString());
        addAndMakeVisible(section.bypassButton);
        section.bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, bypassParamId, section.bypassButton);

        section.bypassLabel.setText("ACTIVE", juce::dontSendNotification);
        section.bypassLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        section.bypassLabel.setJustificationType(juce::Justification::centred);
        section.bypassLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(section.bypassLabel);

        // Knobs / Sliders
        for (const auto& pair : paramIdsAndLabels) {
            auto control = std::make_unique<EffectSection::ParameterControl>();
            control->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            control->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 14);
            control->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            control->slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.8f));
            control->slider.getProperties().set("accentColor", color.toString());
            addAndMakeVisible(control->slider);

            control->label.setText(pair.second, juce::dontSendNotification);
            control->label.setFont(10.0f);
            control->label.setJustificationType(juce::Justification::centred);
            control->label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
            addAndMakeVisible(control->label);

            control->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, pair.first, control->slider);
            section.params.push_back(std::move(control));
        }

        // Setup Sync
        if (hasSync) {
            section.syncButton.setButtonText("Sync");
            section.syncButton.getProperties().set("accentColor", color.toString());
            addAndMakeVisible(section.syncButton);
            section.syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, syncParamId, section.syncButton);

            section.syncNoteSelector.addItemList(juce::StringArray{"1/32", "1/16t", "1/16", "1/8t", "1/8", "1/4t", "1/8d", "1/4", "1/4d", "1/2", "1/1"}, 1);
            if (title == "MODULATION") {
                section.syncNoteSelector.clear();
                section.syncNoteSelector.addItemList(juce::StringArray{"8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16"}, 1);
            }
            section.syncNoteSelector.getProperties().set("accentColor", color.toString());
            addAndMakeVisible(section.syncNoteSelector);
            section.syncNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, noteParamId, section.syncNoteSelector);
        }
    };

    // ── Build Modulation Section ─────────────────────────────────────────────
    std::vector<std::pair<juce::String, juce::String>> modParams = {
        {"modSpeed", "SPEED"}, {"modDepth", "DEPTH"}, {"modMix", "MIX"},
        {"modTone", "TONE"}, {"modP1", "P1"}, {"modP2", "P2"}, {"modLevel", "LEVEL"}
    };
    setupSection(modSection_, "MODULATION", modColor,
                 juce::StringArray{"Chorus", "Flanger", "Rotary", "Vibe", "Phaser", "Vintage Trem"},
                 "bypassMod", "modeMod", modParams, true, "modTempoSync", "modNoteDiv");

    // ── Build Delay Section ──────────────────────────────────────────────────
    std::vector<std::pair<juce::String, juce::String>> dlyParams = {
        {"delayTime", "TIME"}, {"delayRepeats", "REPEATS"}, {"delayMix", "MIX"},
        {"delayFilter", "FILTER"}, {"delayGrit", "GRIT"}, {"delayModSpd", "MOD SPEED"}, {"delayModDep", "MOD DEPTH"}
    };
    setupSection(delaySection_, "DELAY", delayColor,
                 juce::StringArray{"Digital", "Tape", "Dual", "Filter Dly"},
                 "bypassDelay", "modeDelay", dlyParams, true, "delayTempoSync", "delayNoteDiv");

    // ── Build Reverb Section ─────────────────────────────────────────────────
    std::vector<std::pair<juce::String, juce::String>> revParams = {
        {"reverbDecay", "DECAY"}, {"reverbPreDelay", "PRE-DELAY"}, {"reverbMix", "MIX"},
        {"reverbTone", "TONE"}, {"reverbMod", "MODULATION"}, {"reverbParam1", "PARAM 1"}, {"reverbParam2", "PARAM 2"}
    };
    setupSection(reverbSection_, "REVERB", reverbColor,
                 juce::StringArray{"Room", "Hall", "Plate", "Spring"},
                 "bypassReverb", "modeReverb", revParams, false, "", "");

    // Setup Reverb Hold (metallic switch)
    reverbHoldButton.getProperties().set("isBypassButton", true);
    reverbHoldButton.getProperties().set("accentColor", reverbColor.toString());
    addAndMakeVisible(reverbHoldButton);
    reverbHoldAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "reverbHold", reverbHoldButton);

    reverbHoldLabel.setText("HOLD", juce::dontSendNotification);
    reverbHoldLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    reverbHoldLabel.setJustificationType(juce::Justification::centred);
    reverbHoldLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(reverbHoldLabel);

    // Setup Custom Text Value Formatting for dynamic displays
    // Mod Speed display (Hz vs Sync notes)
    modSection_.params[0]->slider.textFromValueFunction = [this](double val) {
        bool sync = processor.apvts.getRawParameterValue("modTempoSync")->load() > 0.5f;
        if (sync) {
            int noteDiv = (int)processor.apvts.getRawParameterValue("modNoteDiv")->load();
            const char* divs[] = {"8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16"};
            return juce::String(divs[noteDiv]);
        }
        auto mode = static_cast<ModModeId>((int)processor.apvts.getRawParameterValue("modeMod")->load());
        float hz = map_param(val, mod_fx::get_param_range(mode, mod_fx::ParamId::Speed));
        return juce::String(hz, 2) + " Hz";
    };

    // Mod Param 2 custom enum string formatter
    modSection_.params[5]->slider.textFromValueFunction = [this](double val) {
        auto mode = static_cast<ModModeId>((int)processor.apvts.getRawParameterValue("modeMod")->load());
        switch (mode) {
            case ModModeId::Phaser: {
                static const char* k[] = {"2 ST","4 ST","6 ST","8 ST","12 ST","16 ST","BARBER"};
                int i = (int)(val * 6.999f);
                return juce::String(k[i < 7 ? i : 6]);
            }
            case ModModeId::Chorus: {
                static const char* k[] = {"dBUCKET","MULTI","VIBRATO","DETUNE","DIGITAL"};
                int i = (int)(val * 4.999f);
                return juce::String(k[i < 5 ? i : 4]);
            }
            case ModModeId::Flanger: {
                static const char* k[] = {"SILVER","GREY","BLACK+","BLACK-","ZERO+","ZERO-"};
                int i = (int)(val * 5.999f);
                return juce::String(k[i < 6 ? i : 5]);
            }
            case ModModeId::VintTrem: {
                static const char* k[] = {"TUBE","HARMONIC","PHOTO"};
                int i = (int)(val * 2.999f);
                return juce::String(k[i < 3 ? i : 2]);
            }
            default:
                return juce::String(val, 2);
        }
    };

    // Delay Time display (ms vs Sync notes)
    delaySection_.params[0]->slider.textFromValueFunction = [this](double val) {
        bool sync = processor.apvts.getRawParameterValue("delayTempoSync")->load() > 0.5f;
        if (sync) {
            int noteDiv = (int)processor.apvts.getRawParameterValue("delayNoteDiv")->load();
            const char* divs[] = {"1/32", "1/16t", "1/16", "1/8t", "1/8", "1/4t", "1/8d", "1/4", "1/4d", "1/2", "1/1"};
            return juce::String(divs[noteDiv]);
        }
        auto mode = static_cast<DelayModeId>((int)processor.apvts.getRawParameterValue("modeDelay")->load());
        float secs = map_param(val, delay_fx::get_param_range(mode, delay_fx::ParamId::Time));
        return juce::String(secs * 1000.0f, 0) + " ms";
    };

    // Reverb Decay display
    reverbSection_.params[0]->slider.textFromValueFunction = [this](double val) {
        auto mode = static_cast<ReverbModeId>((int)processor.apvts.getRawParameterValue("modeReverb")->load());
        float decay = map_param(val, reverb_fx::get_param_range(mode, reverb_fx::ParamId::Decay));
        return juce::String(decay, 1) + " s";
    };

    // Reverb Pre-Delay display
    reverbSection_.params[1]->slider.textFromValueFunction = [this](double val) {
        float secs = map_param(val, reverb_fx::default_ranges::PRE_DELAY);
        return juce::String(secs * 1000.0f, 0) + " ms";
    };

    setSize (1000, 600);
    startTimer (100); // Check and update dynamic GUI every 100ms
}

DaisyMultiFxAudioProcessorEditor::~DaisyMultiFxAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void DaisyMultiFxAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient (futuristic dark style)
    juce::ColourGradient backgroundGrad(juce::Colour(24, 24, 28), 0.0f, 0.0f,
                                       juce::Colour(12, 12, 14), 0.0f, getHeight(), false);
    g.setGradientFill(backgroundGrad);
    g.fillAll();

    // Glassmorphism title block background
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    g.fillRoundedRectangle(20.0f, 15.0f, getWidth() - 40.0f, 65.0f, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(20.0f, 15.0f, getWidth() - 40.0f, 65.0f, 8.0f, 1.0f);

    // Title text
    g.setFont(juce::Font("Outfit", 26.0f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText("DAISY SEED MULTI-FX PEDAL", 40, 20, 400, 50, juce::Justification::centredLeft);

    g.setFont(juce::Font("Inter", 12.0f, juce::Font::plain));
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawText("DSP Verification Desktop Client (48kHz block parity)", getWidth() - 360, 20, 320, 50, juce::Justification::centredRight);

    // Cards / Section Glassmorphism Backgrounds
    auto drawCard = [&g](const juce::Rectangle<int>& rect, juce::Colour accent) {
        g.setColour(juce::Colour(20, 20, 22).withAlpha(0.6f));
        g.fillRoundedRectangle(rect.toFloat(), 12.0f);
        
        // Glow outline
        g.setColour(accent.withAlpha(0.12f));
        g.drawRoundedRectangle(rect.toFloat().expanded(1.0f), 12.0f, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(rect.toFloat(), 12.0f, 1.0f);
    };

    drawCard(modSection_.backgroundCard.getBounds(), juce::Colour(0, 229, 255));
    drawCard(delaySection_.backgroundCard.getBounds(), juce::Colour(255, 145, 0));
    drawCard(reverbSection_.backgroundCard.getBounds(), juce::Colour(213, 0, 249));

    // Signal chain visualizer
    drawSignalChain(g);
}

void DaisyMultiFxAudioProcessorEditor::drawSignalChain(juce::Graphics& g) {
    bool modBypassed = processor.apvts.getRawParameterValue("bypassMod")->load() > 0.5f;
    bool dlyBypassed = processor.apvts.getRawParameterValue("bypassDelay")->load() > 0.5f;
    bool revBypassed = processor.apvts.getRawParameterValue("bypassReverb")->load() > 0.5f;

    auto drawArrow = [&g](float x, float y, bool active, juce::Colour color) {
        g.setColour(active ? color : juce::Colours::white.withAlpha(0.1f));
        juce::Path p;
        p.startNewSubPath(x, y - 4);
        p.lineTo(x + 8, y);
        p.lineTo(x, y + 4);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        g.setColour(active ? color.withAlpha(0.3f) : juce::Colours::transparentBlack);
        g.fillEllipse(x - 2, y - 5, 10, 10);
    };

    // Link Y = 50.0f (inside title area)
    float centerY = 47.5f;
    float modX = 470.0f;
    float dlyX = 580.0f;
    float revX = 690.0f;

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawText("INPUT", modX - 45, centerY - 10, 40, 20, juce::Justification::centred);

    drawArrow(modX, centerY, !modBypassed, juce::Colour(0, 229, 255));
    g.setColour(!modBypassed ? juce::Colour(0, 229, 255) : juce::Colours::white.withAlpha(0.2f));
    g.drawText("MOD", modX + 15, centerY - 10, 30, 20, juce::Justification::centred);

    drawArrow(dlyX, centerY, !dlyBypassed, juce::Colour(255, 145, 0));
    g.setColour(!dlyBypassed ? juce::Colour(255, 145, 0) : juce::Colours::white.withAlpha(0.2f));
    g.drawText("DELAY", dlyX + 15, centerY - 10, 40, 20, juce::Justification::centred);

    drawArrow(revX, centerY, !revBypassed, juce::Colour(213, 0, 249));
    g.setColour(!revBypassed ? juce::Colour(213, 0, 249) : juce::Colours::white.withAlpha(0.2f));
    g.drawText("REVERB", revX + 15, centerY - 10, 45, 20, juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    drawArrow(revX + 75, centerY, true, juce::Colours::white.withAlpha(0.5f));
    g.drawText("OUTPUT", revX + 90, centerY - 10, 50, 20, juce::Justification::centred);
}

void DaisyMultiFxAudioProcessorEditor::resized() {
    auto setupLayout = [this](EffectSection& section, int startX, int startY, int width, int height) {
        section.backgroundCard.setBounds(startX, startY, width, height);
        section.titleLabel.setBounds(startX + 10, startY + 15, width - 20, 25);
        section.modeSelector.setBounds(startX + 30, startY + 50, width - 60, 25);

        // 7 knobs layout: 2x3 grid + 1 bottom center knob
        int row1Y = startY + 95;
        int row2Y = startY + 205;
        int colW = (width - 20) / 3;

        // Row 1
        for (int i = 0; i < 3; ++i) {
            int cx = startX + 10 + i * colW;
            section.params[i]->slider.setBounds(cx + 2, row1Y, colW - 4, 75);
            section.params[i]->label.setBounds(cx, row1Y - 14, colW, 15);
        }

        // Row 2
        for (int i = 3; i < 6; ++i) {
            int cx = startX + 10 + (i - 3) * colW;
            section.params[i]->slider.setBounds(cx + 2, row2Y, colW - 4, 75);
            section.params[i]->label.setBounds(cx, row2Y - 14, colW, 15);
        }

        // Row 3 (Level knob + Sync settings / Reverb Hold)
        int row3Y = startY + 315;
        
        // Level knob is index 6
        section.params[6]->slider.setBounds(startX + 20, row3Y, 80, 75);
        section.params[6]->label.setBounds(startX + 15, row3Y - 14, 90, 15);

        // Sync button & selector positions
        if (section.syncButton.isVisible()) {
            section.syncButton.setBounds(startX + 130, row3Y + 5, 80, 22);
            section.syncNoteSelector.setBounds(startX + 130, row3Y + 35, 120, 22);
        }

        // Bypass switch at very bottom
        section.bypassButton.setBounds(startX + (width - 44) / 2, startY + 410, 44, 44);
        section.bypassLabel.setBounds(startX, startY + 458, width, 12);
    };

    // Place the 3 panels
    setupLayout(modSection_, 25, 105, 305, 475);
    setupLayout(delaySection_, 347, 105, 305, 475);
    setupLayout(reverbSection_, 670, 105, 305, 475);

    // Reverb Hold stompbox placement (takes space in Reverb's Row 3 instead of Sync)
    int revRow3Y = 105 + 315;
    reverbHoldButton.setBounds(670 + 160, revRow3Y + 5, 44, 44);
    reverbHoldLabel.setBounds(670 + 132, revRow3Y + 53, 100, 12);
}

void DaisyMultiFxAudioProcessorEditor::timerCallback() {
    if (updateDynamicUi()) {
        repaint();
    }
}

bool DaisyMultiFxAudioProcessorEditor::updateDynamicUi() {
    bool needsRepaint = false;

    // 1. Mod section labels override
    int curModMode = (int)processor.apvts.getRawParameterValue("modeMod")->load();
    if (curModMode != lastModMode_) {
        lastModMode_ = curModMode;
        
        struct ModAlgoDesc { const char* p1; const char* p2; };
        static const ModAlgoDesc kModAlgoDesc[6] = {
            {"DELAY",  "TYPE"},    // Chorus
            {"REGEN",  "TYPE"},    // Flanger
            {"DRIVE",  "SPEED"},   // Rotary
            {"REGEN",  "P2"},      // Vibe
            {"REGEN",  "STAGES"},  // Phaser
            {"P1",     "TYPE"},    // VintTrem
        };

        if (curModMode >= 0 && curModMode < 6) {
            modSection_.params[4]->label.setText(kModAlgoDesc[curModMode].p1, juce::dontSendNotification);
            modSection_.params[5]->label.setText(kModAlgoDesc[curModMode].p2, juce::dontSendNotification);
        }
        needsRepaint = true;
    }

    // 2. Reverb section labels override
    int curReverbMode = (int)processor.apvts.getRawParameterValue("modeReverb")->load();
    if (curReverbMode != lastReverbMode_) {
        lastReverbMode_ = curReverbMode;

        static const std::pair<juce::String, juce::String> kDescriptors[4] = {
            {"SIZE", "DIFFUSION"}, // Room
            {"SIZE", "MID EQ"},    // Hall
            {"SIZE", "UNUSED"},    // Plate
            {"DWELL", "SPRINGS"}   // Spring
        };

        if (curReverbMode >= 0 && curReverbMode < 4) {
            reverbSection_.params[5]->label.setText(kDescriptors[curReverbMode].first, juce::dontSendNotification);
            reverbSection_.params[6]->label.setText(kDescriptors[curReverbMode].second, juce::dontSendNotification);
            
            // Hide Plate Param 2 knob since it's unused
            if (curReverbMode == 2) {
                reverbSection_.params[6]->slider.setAlpha(0.2f);
                reverbSection_.params[6]->slider.setEnabled(false);
            } else {
                reverbSection_.params[6]->slider.setAlpha(1.0f);
                reverbSection_.params[6]->slider.setEnabled(true);
            }
        }
        needsRepaint = true;
    }

    // 3. Sync Mode overrides for sliders
    bool delaySync = processor.apvts.getRawParameterValue("delayTempoSync")->load() > 0.5f;
    if (delaySync != lastDelaySync_) {
        lastDelaySync_ = delaySync;
        delaySection_.syncNoteSelector.setEnabled(delaySync);
        delaySection_.params[0]->label.setText(delaySync ? "SYNC" : "TIME", juce::dontSendNotification);
        // Force text refresh
        delaySection_.params[0]->slider.updateText();
        needsRepaint = true;
    }

    bool modSync = processor.apvts.getRawParameterValue("modTempoSync")->load() > 0.5f;
    if (modSync != lastModSync_) {
        lastModSync_ = modSync;
        modSection_.syncNoteSelector.setEnabled(modSync);
        modSection_.params[0]->label.setText(modSync ? "SYNC" : "SPEED", juce::dontSendNotification);
        modSection_.params[0]->slider.updateText();
        needsRepaint = true;
    }

    // 4. Bypass states for signal chain visualizer
    bool modBypassed = processor.apvts.getRawParameterValue("bypassMod")->load() > 0.5f;
    bool dlyBypassed = processor.apvts.getRawParameterValue("bypassDelay")->load() > 0.5f;
    bool revBypassed = processor.apvts.getRawParameterValue("bypassReverb")->load() > 0.5f;
    if (modBypassed != lastModBypassed_ || dlyBypassed != lastDelayBypassed_ || revBypassed != lastReverbBypassed_) {
        lastModBypassed_ = modBypassed;
        lastDelayBypassed_ = dlyBypassed;
        lastReverbBypassed_ = revBypassed;
        needsRepaint = true;
    }

    return needsRepaint;
}

} // namespace pedal
