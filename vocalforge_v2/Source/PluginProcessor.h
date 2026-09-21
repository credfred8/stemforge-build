#pragma once
#include <JuceHeader.h>
#include "VocalEngine.h"

class VocalForgeAudioProcessor : public juce::AudioProcessor
{
public:
    VocalForgeAudioProcessor();
    ~VocalForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VocalForge ONE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return (int) presetNames.size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    VocalEngine engine;
    void applyPreset (int index);
    int getPresetIndex() const { return currentPreset; }
    const std::array<juce::String, 8> presetNames {
        "Male Rap - MID-TOP FORWARD",
        "Male Rap - AGGRESSIVE-GRIT",
        "Male Rap - LOW/CHEST",
        "Boom Bap - DUSTY-DARK",
        "Female - CLEAR AIR",
        "Female - SMOOTH DENSE",
        "Backs - WIDE GLUE",
        "Adlibs - SPACE CUT"
    };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    VocalSettings readSettings() const;
    int currentPreset = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeAudioProcessor)
};
