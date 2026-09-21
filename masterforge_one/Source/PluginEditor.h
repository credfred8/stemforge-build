#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MasterForgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MasterForgeAudioProcessorEditor (MasterForgeAudioProcessor&);
    ~MasterForgeAudioProcessorEditor() override;

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
        juce::String tooltip;
    };

    class ForgeLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ForgeLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool shouldDrawButtonAsDown) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;

        juce::Font getComboBoxFont (juce::ComboBox&) override;
    };

    class AnalyzerPanel : public juce::Component
    {
    public:
        explicit AnalyzerPanel (MasterForgeAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        MasterForgeAudioProcessor& processor;
    };

    class MeterPanel : public juce::Component
    {
    public:
        explicit MeterPanel (MasterForgeAudioProcessor& p) : processor (p) {}
        void paint (juce::Graphics&) override;

    private:
        MasterForgeAudioProcessor& processor;
    };

    class ModulePage : public juce::Component
    {
    public:
        ModulePage (MasterForgeAudioProcessor&,
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

    class ModuleTab : public juce::Component
    {
    public:
        ModuleTab (MasterForgeAudioProcessor&,
                   juce::String title,
                   juce::String toggleId,
                   int index,
                   std::function<void(int)> selectCallback);

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;
        void setSelected (bool shouldBeSelected);

    private:
        MasterForgeAudioProcessor& processor;
        juce::String titleText;
        int moduleIndex = 0;
        bool selected = false;
        std::function<void(int)> onSelect;
        juce::ToggleButton enabled;
        std::unique_ptr<ButtonAttachment> enabledAttachment;
    };

    void timerCallback() override;
    void addModule (juce::String title,
                    juce::String toggleId,
                    juce::String helpText,
                    std::initializer_list<ParamSpec> params);
    void selectModule (int index);

    MasterForgeAudioProcessor& processor;
    ForgeLookAndFeel forgeLookAndFeel;
    AnalyzerPanel analyzer;
    MeterPanel meters;

    juce::ComboBox presetBox;
    juce::ToggleButton masterBypass { "BYPASS" };
    std::unique_ptr<ButtonAttachment> masterBypassAttachment;

    juce::Viewport moduleStripViewport;
    juce::Component moduleStripContent;
    juce::Component moduleDetail;

    std::vector<std::unique_ptr<ModulePage>> pages;
    std::vector<std::unique_ptr<ModuleTab>> tabs;
    int selectedModule = 0;

    juce::TooltipWindow tooltip { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeAudioProcessorEditor)
};
