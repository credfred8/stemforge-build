#pragma once
#include <JuceHeader.h>
#include "MasterEngine.h"

class MasterForgeAudioProcessor : public juce::AudioProcessor
{
public:
    MasterForgeAudioProcessor();
    ~MasterForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MasterForge ONE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) presetNames.size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);
    int getPresetIndex() const { return currentPreset; }

    void setModuleChain (const std::vector<int>& modules);
    std::vector<int> getModuleChain() const;
    static juce::String moduleIdForIndex (int index);
    static int moduleIndexForId (const juce::String& id);

    juce::AudioProcessorValueTreeState apvts;
    MasterEngine engine;

    const std::array<juce::String, 10> presetNames {
        "Boom Bap - DENSE PUNCH",
        "Boom Bap - DUSTY ANALOG",
        "Hip-Hop - MODERN DENSE",
        "Trap - LOUD CLEAN",
        "Streaming - TRANSPARENT",
        "Vinyl - WARM GLUE",
        "Drums - HARD PUNCH",
        "Mixbus - OPEN DYNAMIC",
        "Safe Master - CLEAN",
        "INIT - EMPTY / ALL OFF"
    };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    MasterSettings readSettings() const;
    void resetDefaultChain();

    int currentPreset = 0;
    std::array<std::atomic<int>, forgeModuleCount> chainOrderAtomic {};
    std::atomic<int> chainCountAtomic { forgeModuleCount };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeAudioProcessor)
};
