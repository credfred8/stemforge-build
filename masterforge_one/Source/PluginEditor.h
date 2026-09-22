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
    class SinglePageBrowser : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override;
    };

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void timerCallback() override;
    void sendState();
    void handleSetParam (juce::var payload);
    void handleGesture (juce::var payload);
    void handlePreset (juce::var payload);

    juce::var makeParameterSnapshot() const;
    juce::var makeMeterSnapshot() const;

    MasterForgeAudioProcessor& processor;
    std::unique_ptr<SinglePageBrowser> browser;
    bool browserReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterForgeAudioProcessorEditor)
};
