#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class EQDisplay : public juce::Component, private juce::Timer
{
public:
    EQDisplay (VocalForgeAudioProcessor& p) : proc (p)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff12151b));
        g.fillRoundedRectangle (r, 10.0f);

        g.setColour (juce::Colour (0xff2b313d));
        for (int i = 1; i < 6; ++i)
        {
            auto x = r.getX() + r.getWidth() * (float) i / 6.0f;
            g.drawVerticalLine ((int) x, r.getY() + 8.0f, r.getBottom() - 8.0f);
        }
        for (int i = 1; i < 4; ++i)
        {
            auto y = r.getY() + r.getHeight() * (float) i / 4.0f;
            g.drawHorizontalLine ((int) y, r.getX() + 8.0f, r.getRight() - 8.0f);
        }

        auto* body = proc.apvts.getRawParameterValue ("body");
        auto* presence = proc.apvts.getRawParameterValue ("presence");
        auto* air = proc.apvts.getRawParameterValue ("air");
        auto* lowcut = proc.apvts.getRawParameterValue ("lowcut");

        juce::Path p;
        for (int px = 0; px < getWidth(); ++px)
        {
            const float norm = (float) px / juce::jmax (1, getWidth() - 1);
            const float freq = 20.0f * std::pow (1000.0f, norm);
            float db = 0.0f;
            const float lc = lowcut->load();
            if (freq < lc)
                db -= juce::jlimit (0.0f, 24.0f, 24.0f * std::log2 (lc / juce::jmax (20.0f, freq)));
            auto bell = [] (float f, float c, float width)
            {
                const float x = std::log2 (f / c) / width;
                return std::exp (-0.5f * x * x);
            };
            db += body->load() * bell (freq, 180.0f, 0.85f);
            db += presence->load() * bell (freq, 3300.0f, 0.72f);
            db += air->load() * juce::jlimit (0.0f, 1.0f, std::log2 (freq / 7000.0f) + 0.5f);
            const float y = juce::jmap (juce::jlimit (-18.0f, 18.0f, db), -18.0f, 18.0f, r.getBottom() - 8.0f, r.getY() + 8.0f);
            if (px == 0) p.startNewSubPath ((float) px, y);
            else p.lineTo ((float) px, y);
        }

        g.setColour (juce::Colour (0xff7dd3fc));
        g.strokePath (p, juce::PathStrokeType (2.2f));

        const float rms = juce::jlimit (0.0f, 1.0f, proc.engine.meterRms.load() * 3.0f);
        const float peak = juce::jlimit (0.0f, 1.0f, proc.engine.meterPeak.load());
        auto meter = r.removeFromRight (10.0f).reduced (2.0f, 6.0f);
        g.setColour (juce::Colour (0xff253041)); g.fillRect (meter);
        g.setColour (juce::Colour (0xff38bdf8));
        g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * rms));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillRect (meter.withTop (meter.getBottom() - meter.getHeight() * peak).withHeight (1.5f));
    }

private:
    void timerCallback() override { repaint(); }
    VocalForgeAudioProcessor& proc;
};

class VocalForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalForgeAudioProcessorEditor (VocalForgeAudioProcessor&);
    ~VocalForgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramID, const juce::String& text, const juce::String& suffix = {});
    VocalForgeAudioProcessor& processor;
    EQDisplay eqDisplay;
    juce::ComboBox presetBox;

    std::array<Knob, 12> knobs;
    const std::array<juce::String,12> ids { "lowcut","body","presence","air","deess","comp","leveler","parallel","sat","grit","doubler","width" };
    const std::array<juce::String,12> names { "LOW CUT","BODY","PRESENCE","AIR","DE-ESS","PEAK COMP","LEVELER","PARALLEL","SATURATION","GRIT","DOUBLER","WIDTH" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeAudioProcessorEditor)
};
