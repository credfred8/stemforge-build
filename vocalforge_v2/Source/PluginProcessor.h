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

    const juce::String getName() const override { return "VocalForge ONE 2.2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Keep host program handling intentionally simple.
    // Factory presets live inside the plugin UI, avoiding host re-entrancy
    // during FL Studio's plugin initialisation/state restore.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
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
    std::atomic<bool> prepared { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeAudioProcessor)
};
