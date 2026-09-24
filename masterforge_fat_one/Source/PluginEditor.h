#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MasterForgeFatOneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit MasterForgeFatOneAudioProcessorEditor (MasterForgeFatOneAudioProcessor&);
    ~MasterForgeFatOneAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    class FatLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                               float, float, juce::Slider&) override;
    };

    MasterForgeFatOneAudioProcessor& processor;
    FatLookAndFeel laf;
    juce::Slider fat;
    juce::Label valueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float inPeak = 0.0f, outPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeFatOneAudioProcessorEditor)
};
