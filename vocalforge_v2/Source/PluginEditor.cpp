#include "PluginEditor.h"

VocalForgeAudioProcessorEditor::VocalForgeAudioProcessorEditor (VocalForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), eqDisplay (p)
{
    setResizable (true, true);
    setResizeLimits (760, 520, 1500, 980);
    setSize (980, 680);

    addAndMakeVisible (eqDisplay);

    presetBox.setTextWhenNothingSelected ("Preset");
    for (int i = 0; i < (int) processor.presetNames.size(); ++i)
        presetBox.addItem (processor.presetNames[(size_t) i], i + 1);
    presetBox.setSelectedItemIndex (processor.getPresetIndex(), juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedItemIndex() >= 0)
            processor.applyPreset (presetBox.getSelectedItemIndex());
    };
    addAndMakeVisible (presetBox);

    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], ids[i], names[i], ids[i] == "lowcut" ? " Hz" : "");
}

void VocalForgeAudioProcessorEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& text, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 19);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff38bdf8));
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff293241));
    k.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colour (0xffcbd5e1));
    k.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, id, k.slider);
}

void VocalForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff090b10));
    auto r = getLocalBounds().toFloat().reduced (18.0f);

    g.setColour (juce::Colour (0xfff8fafc));
    g.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
    g.drawText ("VOCALFORGE ONE", r.removeFromTop (34.0f), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff64748b));
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("PRO VOCAL CHANNEL / DENSITY / WIDTH / PRINT CONTROL", 22, 49, getWidth() - 44, 18, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff171b23));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (10.0f), 14.0f, 1.0f);
}

void VocalForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (64);

    auto top = area.removeFromTop (42);
    presetBox.setBounds (top.removeFromLeft (juce::jmin (520, top.getWidth())).reduced (0, 4));

    area.removeFromTop (8);
    eqDisplay.setBounds (area.removeFromTop ((int) (getHeight() * 0.31f)));

    area.removeFromTop (10);
    const int cols = 6;
    const int rows = 2;
    const int cellW = area.getWidth() / cols;
    const int cellH = juce::jmax (110, area.getHeight() / rows);

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH).reduced (5);
        knobs[(size_t) i].label.setBounds (cell.removeFromTop (20));
        knobs[(size_t) i].slider.setBounds (cell);
    }
}
