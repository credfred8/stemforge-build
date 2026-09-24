#include "PluginEditor.h"

MasterForgeFatOneAudioProcessorEditor::MasterForgeFatOneAudioProcessorEditor (MasterForgeFatOneAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (520, 620);
    setResizable (false, false);

    fat.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    fat.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fat.setLookAndFeel (&laf);
    addAndMakeVisible (fat);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    valueLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    addAndMakeVisible (valueLabel);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "fat", fat);

    startTimerHz (30);
}

void MasterForgeFatOneAudioProcessorEditor::timerCallback()
{
    inPeak = 0.82f * inPeak + 0.18f * processor.getInputPeak();
    outPeak = 0.82f * outPeak + 0.18f * processor.getOutputPeak();
    valueLabel.setText (juce::String (fat.getValue(), 0) + "%", juce::dontSendNotification);
    repaint();
}

void MasterForgeFatOneAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff08090d), 0.0f, 0.0f,
                             juce::Colour (0xff171923), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (juce::Colour (0xfff5f5f7));
    g.setFont (juce::FontOptions (29.0f, juce::Font::bold));
    g.drawFittedText ("MASTERFORGE", 30, 24, getWidth() - 60, 40, juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff808795));
    g.setFont (juce::FontOptions (12.5f, juce::Font::plain));
    g.drawFittedText ("FAT ONE  •  GOLDEN LIGHT AUDIO", 30, 61, getWidth() - 60, 26, juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xfff5f5f7));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawFittedText ("FAT / DENSITY", 30, 410, getWidth() - 60, 28, juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff8d93a1));
    g.setFont (juce::FontOptions (12.0f, juce::Font::plain));
    g.drawFittedText ("LOUDER  •  THICKER  •  WIDER  •  CONTROLLED", 30, 446, getWidth() - 60, 24, juce::Justification::centred, 1);

    auto meter = [&] (juce::Rectangle<float> r, float peak, juce::String label)
    {
        g.setColour (juce::Colour (0xff242733));
        g.fillRoundedRectangle (r, 4.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, peak);
        auto fill = r;
        fill.setWidth (r.getWidth() * norm);
        g.setColour (juce::Colour (0xffd7d9df));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (juce::Colour (0xff7d8492));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (label, r.toNearestInt().translated (0, -18), juce::Justification::centredLeft);
    };

    meter ({ 80.0f, 520.0f, 160.0f, 8.0f }, inPeak,  "IN");
    meter ({ 280.0f, 520.0f, 160.0f, 8.0f }, outPeak, "OUT");

    g.setColour (juce::Colour (0xff5d6370));
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText ("Safety ceiling: -0.85 dBFS", 30, 555, getWidth() - 60, 22, juce::Justification::centred, 1);
}

void MasterForgeFatOneAudioProcessorEditor::resized()
{
    fat.setBounds (105, 100, 310, 310);
    valueLabel.setBounds (190, 365, 140, 42);
}

void MasterForgeFatOneAudioProcessorEditor::FatLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (22.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colour (0xff242733));
    g.fillEllipse (bounds);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius + 10.0f, radius + 10.0f, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff363a45));
    g.strokePath (arc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius + 10.0f, radius + 10.0f, 0.0f,
                            rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xfff0f1f3));
    g.strokePath (valueArc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ColourGradient knob (juce::Colour (0xff414650), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff15171c), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (knob);
    g.fillEllipse (bounds.reduced (12.0f));

    juce::Path pointer;
    const float pointerLength = radius * 0.68f;
    const float pointerThickness = 4.0f;
    pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.62f,
                                 pointerThickness, pointerLength, 2.0f);
    g.setColour (juce::Colour (0xfff7f7f8));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}
