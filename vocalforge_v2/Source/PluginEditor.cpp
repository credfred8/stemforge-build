#include "PluginEditor.h"

using namespace VocalForgeUI;

void AnalyzerPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 12.0f);

    auto plot = r.reduced (16.0f);
    auto meterStrip = plot.removeFromRight (14.0f);
    plot.removeFromRight (8.0f);

    g.setColour (border.withAlpha (0.65f));
    for (int i = 1; i < 8; ++i)
    {
        const auto x = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    for (int i = 1; i < 6; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * (float) i / 6.0f;
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    g.setColour (muted.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    const std::array<const char*, 7> fLabels { "50", "100", "250", "1k", "4k", "10k", "20k" };
    for (int i = 0; i < 7; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 6.0f;
        g.drawText (fLabels[(size_t)i], (int)x - 18, (int)plot.getBottom() - 16, 36, 14, juce::Justification::centred);
    }

    auto* body = proc.apvts.getRawParameterValue ("body");
    auto* presence = proc.apvts.getRawParameterValue ("presence");
    auto* air = proc.apvts.getRawParameterValue ("air");
    auto* lowcut = proc.apvts.getRawParameterValue ("lowcut");

    juce::Path curve;
    juce::Path glow;
    for (int px = 0; px < (int) plot.getWidth(); ++px)
    {
        const float norm = (float) px / juce::jmax (1, (int)plot.getWidth() - 1);
        const float freq = 20.0f * std::pow (1000.0f, norm);
        float db = 0.0f;
        const float lc = lowcut->load();

        if (freq < lc)
            db -= juce::jlimit (0.0f, 24.0f, 24.0f * std::log2 (lc / juce::jmax (20.0f, freq)));

        auto bell = [] (float f, float c, float w)
        {
            const float x = std::log2 (f / c) / w;
            return std::exp (-0.5f * x * x);
        };

        db += body->load() * bell (freq, 180.0f, 0.85f);
        db += presence->load() * bell (freq, 3300.0f, 0.72f);
        db += air->load() * juce::jlimit (0.0f, 1.0f, std::log2 (freq / 7000.0f) + 0.5f);

        const float y = juce::jmap (juce::jlimit (-18.0f, 18.0f, db),
                                    -18.0f, 18.0f, plot.getBottom() - 20.0f, plot.getY() + 8.0f);
        const float x = plot.getX() + (float) px;
        if (px == 0) { curve.startNewSubPath (x, y); glow.startNewSubPath (x, y); }
        else { curve.lineTo (x, y); glow.lineTo (x, y); }
    }

    g.setColour (cyan.withAlpha (0.15f));
    g.strokePath (glow, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved));
    juce::ColourGradient grad (cyan2, plot.getX(), plot.getCentreY(), violet, plot.getRight(), plot.getCentreY(), false);
    g.setGradientFill (grad);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));

    struct Node { float x, db; juce::Colour c; const char* label; };
    const std::array<Node, 4> nodes {{
        { 0.22f, body->load(), cyan2, "BODY" },
        { 0.60f, presence->load(), violet, "PRES" },
        { 0.84f, air->load(), amber, "AIR" },
        { 0.08f, -8.0f, green, "HP" }
    }};

    for (const auto& n : nodes)
    {
        const float x = plot.getX() + plot.getWidth() * n.x;
        const float y = juce::jmap (juce::jlimit (-12.0f, 12.0f, n.db), -12.0f, 12.0f,
                                    plot.getBottom() - 22.0f, plot.getY() + 12.0f);
        g.setColour (n.c.withAlpha (0.22f));
        g.fillEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f);
        g.setColour (n.c);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText (n.label, (int)x - 24, (int)y + 8, 48, 14, juce::Justification::centred);
    }

    g.setColour (border);
    g.fillRoundedRectangle (meterStrip, 3.0f);
    const float rms = juce::jlimit (0.0f, 1.0f, proc.engine.meterRms.load() * 3.0f);
    const float peak = juce::jlimit (0.0f, 1.0f, proc.engine.meterPeak.load());
    auto fill = meterStrip.reduced (2.0f);
    g.setColour (green);
    g.fillRoundedRectangle (fill.withTop (fill.getBottom() - fill.getHeight() * rms), 2.0f);
    g.setColour (text);
    g.fillRect (fill.withTop (fill.getBottom() - fill.getHeight() * peak).withHeight (1.0f));
}

void MeterPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 12.0f);

    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("OUTPUT", 0, 10, getWidth(), 16, juce::Justification::centred);

    auto meter = r.reduced (18.0f, 38.0f);
    g.setColour (bg0);
    g.fillRoundedRectangle (meter, 5.0f);

    const float peak = juce::jlimit (0.0f, 1.0f, proc.engine.meterPeak.load());
    const float rms  = juce::jlimit (0.0f, 1.0f, proc.engine.meterRms.load() * 3.0f);
    auto inner = meter.reduced (4.0f);
    auto rmsRect = inner.withTop (inner.getBottom() - inner.getHeight() * rms);
    juce::ColourGradient grad (green, inner.getBottomLeft(), amber, inner.getTopLeft(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rmsRect, 3.0f);

    g.setColour (text);
    g.fillRect (inner.withTop (inner.getBottom() - inner.getHeight() * peak).withHeight (2.0f));

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("0", 0, (int)meter.getY() - 5, getWidth(), 14, juce::Justification::centred);
    g.drawText ("-12", 0, (int)meter.getCentreY() - 7, getWidth(), 14, juce::Justification::centred);
    g.drawText ("-INF", 0, (int)meter.getBottom() - 7, getWidth(), 14, juce::Justification::centred);
}

VocalForgeAudioProcessorEditor::VocalForgeAudioProcessorEditor (VocalForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), analyzer (p), meters (p)
{
    setLookAndFeel (&laf);
    setResizable (true, true);
    setResizeLimits (980, 640, 1680, 1080);
    setSize (1280, 790);

    brand.setText ("VOCALFORGE ONE", juce::dontSendNotification);
    brand.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    brand.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (brand);

    edition.setText ("PRO VOCAL SUITE  /  v2.1", juce::dontSendNotification);
    edition.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    edition.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (edition);

    presetBox.setTextWhenNothingSelected ("Select preset");
    for (int i = 0; i < (int) processor.presetNames.size(); ++i)
        presetBox.addItem (processor.presetNames[(size_t)i], i + 1);
    presetBox.setSelectedItemIndex (processor.getPresetIndex(), juce::dontSendNotification);
    presetBox.setTooltip ("Factory vocal presets. Each preset sets the complete chain as a starting point.");
    presetBox.onChange = [this]
    {
        const auto i = presetBox.getSelectedItemIndex();
        if (i >= 0) processor.applyPreset (i);
    };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (abButton);
    addAndMakeVisible (globalBypass);
    abButton.setTooltip ("Compare two settings quickly.");
    globalBypass.setTooltip ("Global bypass for the whole VocalForge chain.");

    chainLabel.setText ("SIGNAL CHAIN", juce::dontSendNotification);
    chainLabel.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    chainLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (chainLabel);

    for (int i = 0; i < (int)modules.size(); ++i)
    {
        modules[(size_t)i] = std::make_unique<ModuleTile> (moduleNames[(size_t)i], moduleTips[(size_t)i]);
        modules[(size_t)i]->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        modules[(size_t)i]->addMouseListener (this, false);
        addAndMakeVisible (*modules[(size_t)i]);
    }

    sectionTitle.setText ("PARAMETRIC EQ", juce::dontSendNotification);
    sectionTitle.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    sectionTitle.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (sectionTitle);

    sectionDescription.setText ("Shape the vocal before the final dynamics. Keep the center clear and use air only when the source can take it.", juce::dontSendNotification);
    sectionDescription.setFont (juce::Font (juce::FontOptions (10.0f)));
    sectionDescription.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (sectionDescription);

    addAndMakeVisible (analyzer);
    addAndMakeVisible (meters);

    for (int i = 0; i < (int)params.size(); ++i)
        setupParam (params[(size_t)i], paramIds[(size_t)i], paramNames[(size_t)i], moduleTips[(size_t)i],
                    paramIds[(size_t)i] == "lowcut" ? " Hz" :
                    (paramIds[(size_t)i] == "gate" || paramIds[(size_t)i] == "output" ? " dB" : ""));

    setModuleSelected (2);
}

VocalForgeAudioProcessorEditor::~VocalForgeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VocalForgeAudioProcessorEditor::setupParam (ParamControl& c, const juce::String& id,
                                                  const juce::String& titleText,
                                                  const juce::String& tooltip,
                                                  const juce::String& suffix)
{
    c.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    c.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 21);
    c.slider.setTextValueSuffix (suffix);
    c.slider.setTooltip (tooltip);
    addAndMakeVisible (c.slider);

    c.title.setText (titleText, juce::dontSendNotification);
    c.title.setJustificationType (juce::Justification::centred);
    c.title.setColour (juce::Label::textColourId, text);
    c.title.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    addAndMakeVisible (c.title);

    c.valueHint.setText ("?", juce::dontSendNotification);
    c.valueHint.setTooltip (tooltip);
    c.valueHint.setJustificationType (juce::Justification::centred);
    c.valueHint.setColour (juce::Label::textColourId, muted);
    c.valueHint.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    addAndMakeVisible (c.valueHint);

    c.attachment = std::make_unique<SliderAttachment> (processor.apvts, id, c.slider);
}

void VocalForgeAudioProcessorEditor::setModuleSelected (int index)
{
    selectedModule = juce::jlimit (0, (int)modules.size() - 1, index);
    sectionTitle.setText (moduleNames[(size_t)selectedModule], juce::dontSendNotification);
    sectionDescription.setText (moduleTips[(size_t)selectedModule], juce::dontSendNotification);
    repaint();
}

void VocalForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (bg0, 0.0f, 0.0f, bg1, 0.0f, (float)getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (border.withAlpha (0.65f));
    g.drawHorizontalLine (72, 18.0f, (float)getWidth() - 18.0f);

    auto chain = juce::Rectangle<float> (18.0f, 96.0f, (float)getWidth() - 36.0f, 118.0f);
    g.setColour (panel.withAlpha (0.72f));
    g.fillRoundedRectangle (chain, 12.0f);
    g.setColour (border.withAlpha (0.8f));
    g.drawRoundedRectangle (chain, 12.0f, 1.0f);

    auto bottom = juce::Rectangle<float> (18.0f, (float)getHeight() - 214.0f, (float)getWidth() - 36.0f, 194.0f);
    g.setColour (panel.withAlpha (0.72f));
    g.fillRoundedRectangle (bottom, 12.0f);
    g.setColour (border.withAlpha (0.8f));
    g.drawRoundedRectangle (bottom, 12.0f, 1.0f);

    g.setColour (cyan.withAlpha (0.85f));
    g.fillRoundedRectangle (18.0f, 17.0f, 4.0f, 35.0f, 2.0f);
}

void VocalForgeAudioProcessorEditor::resized()
{
    auto top = juce::Rectangle<int> (18, 12, getWidth() - 36, 52);
    brand.setBounds (top.removeFromLeft (290));
    edition.setBounds (300, 33, 230, 20);

    auto right = juce::Rectangle<int> (getWidth() - 555, 16, 537, 38);
    globalBypass.setBounds (right.removeFromRight (92));
    right.removeFromRight (8);
    abButton.setBounds (right.removeFromRight (66));
    right.removeFromRight (8);
    presetBox.setBounds (right);

    chainLabel.setBounds (26, 78, 140, 18);

    auto chainArea = juce::Rectangle<int> (26, 108, getWidth() - 52, 94);
    const int gap = 7;
    const int tileW = (chainArea.getWidth() - gap * ((int)modules.size() - 1)) / (int)modules.size();
    for (int i = 0; i < (int)modules.size(); ++i)
    {
        modules[(size_t)i]->setBounds (chainArea.getX() + i * (tileW + gap), chainArea.getY(), tileW, 58);
    }

    auto workspace = juce::Rectangle<int> (18, 226, getWidth() - 36, getHeight() - 460);
    auto meterW = juce::jlimit (80, 120, getWidth() / 12);
    meters.setBounds (workspace.removeFromRight (meterW));
    workspace.removeFromRight (10);
    analyzer.setBounds (workspace);

    sectionTitle.setBounds (28, getHeight() - 208, 220, 24);
    sectionDescription.setBounds (250, getHeight() - 208, getWidth() - 280, 24);

    auto controlsArea = juce::Rectangle<int> (28, getHeight() - 176, getWidth() - 56, 145);
    const int cellW = controlsArea.getWidth() / (int)params.size();
    for (int i = 0; i < (int)params.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (controlsArea.getX() + i * cellW, controlsArea.getY(), cellW, controlsArea.getHeight()).reduced (3);
        params[(size_t)i].title.setBounds (cell.removeFromTop (19));
        params[(size_t)i].valueHint.setBounds (cell.getRight() - 18, cell.getY() - 18, 16, 16);
        params[(size_t)i].slider.setBounds (cell);
    }
}
