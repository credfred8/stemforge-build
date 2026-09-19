#pragma once

#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"
#include "StemEngine.h"

class StemForgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::FileDragAndDropTarget,
                                            private juce::Timer
{
public:
    explicit StemForgeAudioProcessorEditor(StemForgeAudioProcessor&);
    ~StemForgeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    class ForgeLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        ForgeLookAndFeel();
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    };

    void timerCallback() override;
    void chooseInput();
    void chooseOutputFolder();
    void startSeparation();
    void setInputFile(const juce::File& file);
    void updateFileLabel();
    stemforge::SeparationOptions currentOptions() const;

    StemForgeAudioProcessor& processor;
    ForgeLookAndFeel lookAndFeel;
    stemforge::StemEngine engine;

    juce::Label title;
    juce::Label subtitle;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label statusLabel;
    juce::Label hintLabel;

    juce::TextButton loadButton { "Загрузить аудио" };
    juce::TextButton outputButton { "Папка экспорта" };
    juce::TextButton separateButton { "РАЗДЕЛИТЬ HQ" };
    juce::TextButton openFolderButton { "Открыть результат" };

    std::array<juce::ToggleButton, stemforge::stemCount> stemButtons;
    juce::ToggleButton cleanButton { "Clean Sample: убрать отмеченные стемы" };

    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File inputFile;
    juce::File outputDirectory;
    juce::File latestOutputDirectory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemForgeAudioProcessorEditor)
};
