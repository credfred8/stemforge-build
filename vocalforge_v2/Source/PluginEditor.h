#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PremiumUI.h"

class AnalyzerPanel : public juce::Component, private juce::Timer
{
public:
    explicit AnalyzerPanel (VocalForgeAudioProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    VocalForgeAudioProcessor& proc;
};

class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (VocalForgeAudioProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    VocalForgeAudioProcessor& proc;
};

class VocalForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalForgeAudioProcessorEditor (VocalForgeAudioProcessor&);
    ~VocalForgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct ParamControl
    {
        juce::Slider slider;
        juce::Label title;
        juce::Label valueHint;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupParam (ParamControl&, const juce::String& id, const juce::String& title,
                     const juce::String& tooltip, const juce::String& suffix = {});
    void setModuleSelected (int index);

    VocalForgeAudioProcessor& processor;
    VocalForgeUI::PremiumLookAndFeel laf;

    juce::Label brand, edition, chainLabel, sectionTitle, sectionDescription;
    juce::ComboBox presetBox;
    juce::TextButton abButton { "A/B" }, globalBypass { "BYPASS" };

    AnalyzerPanel analyzer;
    MeterPanel meters;

    std::array<std::unique_ptr<VocalForgeUI::ModuleTile>, 12> modules;
    std::array<ParamControl, 12> params;

    const std::array<juce::String, 12> moduleNames {
        "CLEAN", "GATE", "EQ", "DE-ESS", "PEAK COMP", "LEVELER",
        "PARALLEL", "SATURATION", "DOUBLER", "SPACE", "WIDTH", "LIMIT"
    };

    const std::array<juce::String, 12> moduleTips {
        "Cleans low-frequency rumble and unwanted buildup before dynamics.",
        "Reduces room noise and headphone bleed between phrases.",
        "Shapes body, presence and air. Central graph shows the active curve.",
        "Controls harsh S and T consonants without dulling the whole vocal.",
        "Fast peak compression for punch, stability and close-up vocal density.",
        "Slower leveling stage that keeps phrases sitting consistently in the mix.",
        "Adds a heavily compressed signal in parallel for density without flattening transients.",
        "Adds controlled harmonics and grit to make the vocal feel larger and more expensive.",
        "Creates short offset voices that can be blended under the lead to add width and thickness.",
        "Delay and short ambience for depth while keeping the vocal forward.",
        "Controls stereo spread after modulation and ambience.",
        "Final peak protection and output control before the DAW channel."
    };

    const std::array<juce::String, 12> paramIds {
        "lowcut","gate","body","deess","comp","leveler",
        "parallel","sat","doubler","reverb","width","output"
    };

    const std::array<juce::String, 12> paramNames {
        "LOW CUT","THRESHOLD","BODY","DE-ESS","PEAK COMP","LEVELER",
        "PARALLEL","SATURATION","DOUBLER MIX","AMBIENCE","WIDTH","OUTPUT"
    };

    int selectedModule = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeAudioProcessorEditor)
};
