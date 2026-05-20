#include "PluginProcessor.h"
#include "PluginEditor.h"

// Include DSP headers
#include "dsp/fast_math.h"
#include "params/mod_param_map.h"
#include "params/delay_param_map.h"
#include "params/reverb_param_map.h"

namespace pedal {

// Helper to translate note divisions to beats
static float getNoteDurationBeats(int noteDivIndex) {
    // Choices: 1/32, 1/16t, 1/16, 1/8t, 1/8, 1/4t, 1/8d, 1/4, 1/4d, 1/2, 1/1
    switch (noteDivIndex) {
        case 0:  return 0.125f; // 1/32
        case 1:  return 0.1667f; // 1/16t
        case 2:  return 0.25f;  // 1/16
        case 3:  return 0.3333f; // 1/8t
        case 4:  return 0.5f;   // 1/8
        case 5:  return 0.6667f; // 1/4t
        case 6:  return 0.75f;  // 1/8d
        case 7:  return 1.0f;   // 1/4
        case 8:  return 1.5f;   // 1/4d
        case 9:  return 2.0f;   // 1/2
        case 10: return 4.0f;   // 1/1
        default: return 1.0f;
    }
}

static float getModNoteDurationBeats(int noteDivIndex) {
    // Choices: 8 bars, 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16
    switch (noteDivIndex) {
        case 0: return 32.0f; // 8 bars
        case 1: return 16.0f; // 4 bars
        case 2: return 8.0f;  // 2 bars
        case 3: return 4.0f;  // 1 bar (4 beats)
        case 4: return 2.0f;  // 1/2 (2 beats)
        case 5: return 1.0f;  // 1/4 (1 beat)
        case 6: return 0.5f;  // 1/8
        case 7: return 0.25f; // 1/16
        default: return 4.0f;
    }
}

DaisyMultiFxAudioProcessor::DaisyMultiFxAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#else
     : apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Initialize registries
    mod_registry_.Init();
    delay_registry_.Init();
    reverb_registry_.Init();

    // Default modes
    active_mod_ = mod_registry_.get(ModModeId::Chorus);
    active_delay_ = delay_registry_.get(DelayModeId::Tape);
    active_reverb_ = reverb_registry_.get(ReverbModeId::Hall);
}

DaisyMultiFxAudioProcessor::~DaisyMultiFxAudioProcessor()
{
}

const juce::String DaisyMultiFxAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DaisyMultiFxAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DaisyMultiFxAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DaisyMultiFxAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DaisyMultiFxAudioProcessor::getTailLengthSeconds() const
{
    return 20.0; // max reverb decay
}

int DaisyMultiFxAudioProcessor::getNumPrograms()
{
    return 1;
}

int DaisyMultiFxAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DaisyMultiFxAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String DaisyMultiFxAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void DaisyMultiFxAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void DaisyMultiFxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    dawSampleRate_ = sampleRate;

    // Reset components
    mod_registry_.Reset(static_cast<ModModeId>(apvts.getRawParameterValue("modeMod")->load()));
    delay_registry_.Reset(static_cast<DelayModeId>(apvts.getRawParameterValue("modeDelay")->load()));
    reverb_registry_.Reset(static_cast<ReverbModeId>(apvts.getRawParameterValue("modeReverb")->load()));

    sampleCounter_ = 0;
}

void DaisyMultiFxAudioProcessor::releaseResources()
{
}

bool DaisyMultiFxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void DaisyMultiFxAudioProcessor::updateMixCoeffs(float modMix, float dlyMix, float revMix) {
    auto recompute = [](float mix, float& last, float& dry, float& wet, float& norm) {
        if (mix == last) return;
        last              = mix;
        const float angle = mix * 1.57079632679f;
        dry               = fast_cos(angle);
        wet               = fast_sin(angle);
        const float sum   = dry + wet;
        norm              = (sum > 1.0f) ? (1.0f / sum) : 1.0f;
    };

    recompute(modMix, last_mod_mix_, mod_dry_, mod_wet_, mod_norm_);
    recompute(dlyMix, last_dly_mix_, dly_dry_, dly_wet_, dly_norm_);
    recompute(revMix, last_rev_mix_, rev_dry_, rev_wet_, rev_norm_);
}

void DaisyMultiFxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (totalNumInputChannels == 0 || totalNumOutputChannels == 0)
        return;

    // Update tempo info from playhead
    if (auto* playHead = getPlayHead()) {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue() && positionInfo->getBpm().hasValue()) {
            currentBpm_ = *(positionInfo->getBpm());
        }
    }

    int numSamples = buffer.getNumSamples();
    const float* inL = buffer.getReadPointer(0);
    const float* inR = totalNumInputChannels > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0);

    float* outL = buffer.getWritePointer(0);
    float* outR = totalNumOutputChannels > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

    float sr_scale = 48000.0f / static_cast<float>(dawSampleRate_);

    // Cache bypass flags at block boundary (threadsafety and performance)
    bool modBypass = apvts.getRawParameterValue("bypassMod")->load() > 0.5f;
    bool delayBypass = apvts.getRawParameterValue("bypassDelay")->load() > 0.5f;
    bool reverbBypass = apvts.getRawParameterValue("bypassReverb")->load() > 0.5f;

    // Audio loop
    for (int i = 0; i < numSamples; ++i) {
        if (sampleCounter_ == 0) {
            // Read parameter values from APVTS and select active modes
            auto modeMod = static_cast<ModModeId>(apvts.getRawParameterValue("modeMod")->load());
            auto modeDelay = static_cast<DelayModeId>(apvts.getRawParameterValue("modeDelay")->load());
            auto modeReverb = static_cast<ReverbModeId>(apvts.getRawParameterValue("modeReverb")->load());

            if (mod_registry_.get(modeMod) != active_mod_) {
                active_mod_ = mod_registry_.get(modeMod);
                active_mod_->Reset();
            }
            if (delay_registry_.get(modeDelay) != active_delay_) {
                active_delay_ = delay_registry_.get(modeDelay);
                active_delay_->Reset();
            }
            if (reverb_registry_.get(modeReverb) != active_reverb_) {
                active_reverb_ = reverb_registry_.get(modeReverb);
                active_reverb_->Reset();
            }

            // Get parameters
            float modSpeed = apvts.getRawParameterValue("modSpeed")->load();
            float modDepth = apvts.getRawParameterValue("modDepth")->load();
            float modMix = apvts.getRawParameterValue("modMix")->load();
            float modTone = apvts.getRawParameterValue("modTone")->load();
            float modP1 = apvts.getRawParameterValue("modP1")->load();
            float modP2 = apvts.getRawParameterValue("modP2")->load();
            float modLevel = apvts.getRawParameterValue("modLevel")->load();

            float delayTime = apvts.getRawParameterValue("delayTime")->load();
            float delayRepeats = apvts.getRawParameterValue("delayRepeats")->load();
            float delayMix = apvts.getRawParameterValue("delayMix")->load();
            float delayFilter = apvts.getRawParameterValue("delayFilter")->load();
            float delayGrit = apvts.getRawParameterValue("delayGrit")->load();
            float delayModSpd = apvts.getRawParameterValue("delayModSpd")->load();
            float delayModDep = apvts.getRawParameterValue("delayModDep")->load();

            float reverbDecay = apvts.getRawParameterValue("reverbDecay")->load();
            float reverbPreDelay = apvts.getRawParameterValue("reverbPreDelay")->load();
            float reverbMix = apvts.getRawParameterValue("reverbMix")->load();
            float reverbTone = apvts.getRawParameterValue("reverbTone")->load();
            float reverbMod = apvts.getRawParameterValue("reverbMod")->load();
            float reverbParam1 = apvts.getRawParameterValue("reverbParam1")->load();
            float reverbParam2 = apvts.getRawParameterValue("reverbParam2")->load();

            bool reverbHold = apvts.getRawParameterValue("reverbHold")->load() > 0.5f;
            bool delaySync = apvts.getRawParameterValue("delayTempoSync")->load() > 0.5f;
            int delayDiv = (int)apvts.getRawParameterValue("delayNoteDiv")->load();
            bool modSync = apvts.getRawParameterValue("modTempoSync")->load() > 0.5f;
            int modDiv = (int)apvts.getRawParameterValue("modNoteDiv")->load();

            // Mod params building
            mod_params_.speed = map_param(modSpeed, mod_fx::get_param_range(modeMod, mod_fx::ParamId::Speed));
            mod_params_.depth = modDepth;
            mod_params_.mix   = modMix;
            mod_params_.tone  = modTone;
            mod_params_.p1    = modP1;
            mod_params_.p2    = modP2;
            mod_params_.level = map_param(modLevel, mod_fx::default_ranges::LEVEL);
            if (modSync && currentBpm_ > 0.0) {
                float beats = getModNoteDurationBeats(modDiv);
                float secs = beats * (60.0f / (float)currentBpm_);
                mod_params_.speed = 1.0f / secs;
            }
            mod_params_.speed *= sr_scale;

            // Delay params building
            delay_params_.time    = map_param(delayTime, delay_fx::get_param_range(modeDelay, delay_fx::ParamId::Time));
            delay_params_.repeats = map_param(delayRepeats, delay_fx::default_ranges::REPEATS);
            delay_params_.mix     = delayMix;
            delay_params_.filter  = delayFilter;
            delay_params_.grit    = delayGrit;
            delay_params_.mod_spd = map_param(delayModSpd, delay_fx::default_ranges::MOD_SPD);
            delay_params_.mod_dep = delayModDep;
            if (delaySync && currentBpm_ > 0.0) {
                float beats = getNoteDurationBeats(delayDiv);
                delay_params_.time = beats * (60.0f / (float)currentBpm_);
            }
            delay_params_.time /= sr_scale;
            delay_params_.mod_spd *= sr_scale;

            // Reverb params building
            reverb_params_.decay     = map_param(reverbDecay, reverb_fx::get_param_range(modeReverb, reverb_fx::ParamId::Decay));
            reverb_params_.pre_delay = map_param(reverbPreDelay, reverb_fx::default_ranges::PRE_DELAY);
            reverb_params_.mix       = reverbMix;
            reverb_params_.tone      = reverbTone;
            reverb_params_.mod       = reverbMod;
            reverb_params_.param1    = map_param(reverbParam1, reverb_fx::get_param_range(modeReverb, reverb_fx::ParamId::Param1));
            reverb_params_.param2    = map_param(reverbParam2, reverb_fx::get_param_range(modeReverb, reverb_fx::ParamId::Param2));

            // Update coefficients
            updateMixCoeffs(modMix, delayMix, reverbMix);

            // Reverb hold
            if (active_reverb_) active_reverb_->SetHold(reverbHold);

            // Prepare DSP blocks (always prepare to keep filters/LFOs from going stale)
            if (active_mod_)    active_mod_->Prepare(mod_params_);
            if (active_delay_)  active_delay_->Prepare(delay_params_);
            if (active_reverb_) active_reverb_->Prepare(reverb_params_);
        }

        const float dryL = inL[i];
        const float dryR = inR[i];

        // Stage 1: modulation (stereo in/out)
        StereoFrame s1;
        if (active_mod_ && !modBypass) {
            const StereoFrame wet = active_mod_->Process({dryL, dryR}, mod_params_);
            s1.left  = (dryL * mod_dry_ + wet.left  * mod_wet_) * mod_norm_;
            s1.right = (dryR * mod_dry_ + wet.right * mod_wet_) * mod_norm_;
        } else {
            s1 = {dryL, dryR};
        }

        // Stage 2: delay (mono input from s1.left → stereo out)
        StereoFrame s2;
        if (active_delay_ && !delayBypass) {
            const StereoFrame wet = active_delay_->Process(s1.left, delay_params_);
            s2.left  = (s1.left  * dly_dry_ + wet.left  * dly_wet_) * dly_norm_;
            s2.right = (s1.right * dly_dry_ + wet.right * dly_wet_) * dly_norm_;
        } else {
            s2 = s1;
        }

        // Stage 3: reverb (mono input from s2.left → stereo out)
        StereoFrame s3;
        if (active_reverb_ && !reverbBypass) {
            const StereoFrame wet = active_reverb_->Process(s2.left, reverb_params_);
            s3.left  = (s2.left  * rev_dry_ + wet.left  * rev_wet_) * rev_norm_;
            s3.right = (s2.right * rev_dry_ + wet.right * rev_wet_) * rev_norm_;
        } else {
            s3 = s2;
        }

        outL[i] = s3.left;
        outR[i] = s3.right;

        // Advance sample counter (modulo 48)
        sampleCounter_ = (sampleCounter_ + 1) % 48;
    }
}

bool DaisyMultiFxAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* DaisyMultiFxAudioProcessor::createEditor()
{
    return new DaisyMultiFxAudioProcessorEditor (*this);
}

void DaisyMultiFxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DaisyMultiFxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout DaisyMultiFxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bypass toggles
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypassMod", 1), "Bypass Modulation", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypassDelay", 1), "Bypass Delay", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypassReverb", 1), "Bypass Reverb", false));

    // Mode choices
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modeMod", 1), "Modulation Mode",
        juce::StringArray{"Chorus", "Flanger", "Rotary", "Vibe", "Phaser", "VintTrem"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modeDelay", 1), "Delay Mode",
        juce::StringArray{"Digital", "Tape", "Dual", "FiltDly"}, 1)); // Default is Tape
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modeReverb", 1), "Reverb Mode",
        juce::StringArray{"Room", "Hall", "Plate", "Spring"}, 1)); // Default is Hall

    // Sync options
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("delayTempoSync", 1), "Delay Tempo Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("delayNoteDiv", 1), "Delay Note Division",
        juce::StringArray{"1/32", "1/16t", "1/16", "1/8t", "1/8", "1/4t", "1/8d", "1/4", "1/4d", "1/2", "1/1"}, 7)); // Default is 1/4

    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("modTempoSync", 1), "Mod Tempo Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modNoteDiv", 1), "Mod Note Division",
        juce::StringArray{"8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16"}, 3)); // Default is 1 Bar

    // Reverb hold
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("reverbHold", 1), "Reverb Hold", false));

    // 7 params per stage
    // Mod
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modSpeed", 1), "Mod Speed", 0.0f, 1.0f, 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modDepth", 1), "Mod Depth", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modMix", 1), "Mod Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modTone", 1), "Mod Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modP1", 1), "Mod Param 1", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modP2", 1), "Mod Param 2", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("modLevel", 1), "Mod Level", 0.0f, 1.0f, 0.5f)); // mapped [0,2] with default level 1.0 = normalized 0.5

    // Delay
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayTime", 1), "Delay Time", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayRepeats", 1), "Delay Repeats", 0.0f, 1.0f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayMix", 1), "Delay Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayFilter", 1), "Delay Filter", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayGrit", 1), "Delay Grit", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayModSpd", 1), "Delay Mod Speed", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("delayModDep", 1), "Delay Mod Depth", 0.0f, 1.0f, 0.0f));

    // Reverb
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbDecay", 1), "Reverb Decay", 0.0f, 1.0f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbPreDelay", 1), "Reverb PreDelay", 0.0f, 1.0f, 0.04f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbMix", 1), "Reverb Mix", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbTone", 1), "Reverb Tone", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbMod", 1), "Reverb Mod", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbParam1", 1), "Reverb Param 1", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("reverbParam2", 1), "Reverb Param 2", 0.0f, 1.0f, 0.5f));

    return layout;
}

} // namespace pedal

// JUCE plugin entry point helper (expected by juce_audio_plugin_client)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pedal::DaisyMultiFxAudioProcessor();
}
