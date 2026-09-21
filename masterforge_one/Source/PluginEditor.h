#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MasterForgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MasterForgeAudioProcessorEditor (MasterForgeAudioProcessor&);
    ~MasterForgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct ParamSpec
    {
        juce::String id;
        juce::String name;
        juce::String suffix;
    };

    class SpectrumPanel : public juce::Component
    {
    public:
        explicit SpectrumPanel (MasterForgeAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        MasterForgeAudioProcessor& processor;
    };

    class ModuleCard : public juce::Component
    {
    public:
        ModuleCard (MasterForgeAudioProcessor&,
                    juce::String title,
                    juce::String toggleId,
                    juce::String helpText,
                    std::initializer_list<ParamSpec> params);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct ParamControl
        {
            juce::Slider slider;
            juce::Label label;
            std::unique_ptr<SliderAttachment> attachment;
        };

        MasterForgeAudioProcessor& processor;
        juce::String titleText;
        juce::String helpText;
        juce::ToggleButton enabled;
        juce::TextButton help { "?" };
        std::unique_ptr<ButtonAttachment> enabledAttachment;
        std::vector<std::unique_ptr<ParamControl>> controls;
    };

    void timerCallback() override;
    void addModule (juce::String title,
                    juce::String toggleId,
                    juce::String helpText,
                    std::initializer_list<ParamSpec> params);

    MasterForgeAudioProcessor& processor;
    SpectrumPanel spectrum;
    juce::Viewport viewport;
    juce::Component moduleContent;
    std::vector<std::unique_ptr<ModuleCard>> modules;

    juce::ComboBox presetBox;
    juce::ToggleButton masterBypass { "BYPASS" };
    std::unique_ptr<ButtonAttachment> masterBypassAttachment;

    juce::Label inputMeter;
    juce::Label inputPeak;
    juce::Label gainCoach;
    juce::Label outputMeter;
    juce::Label outputPeak;
    juce::Label loudness;
    juce::Label crest;
    juce::Label corr;
    juce::Label status;
    juce::TooltipWindow tooltip { this, 450 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeAudioProcessorEditor)
};
