#include "PluginEditor.h"

namespace
{
juce::Colour bg() { return juce::Colour (0xff070a0f); }
juce::Colour panel() { return juce::Colour (0xff101722); }
juce::Colour panel2() { return juce::Colour (0xff151e2b); }
juce::Colour line() { return juce::Colour (0xff263346); }
juce::Colour cyan() { return juce::Colour (0xff35d7ff); }
juce::Colour violet() { return juce::Colour (0xff9b87ff); }
juce::Colour text() { return juce::Colour (0xffe7eef8); }
juce::Colour muted() { return juce::Colour (0xff8290a4); }
}

void MasterForgeAudioProcessorEditor::SpectrumPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel());
    g.fillRoundedRectangle (r, 13.0f);

    auto plot = r.reduced (14.0f, 12.0f);
    g.setColour (line().withAlpha (0.75f));
    for (int i = 1; i < 8; ++i)
    {
        const auto x = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    for (int i = 1; i < 5; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * (float) i / 5.0f;
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    auto drawSpectrum = [&] (const auto& src, juce::Colour c, float thickness)
    {
        juce::Path p;
        for (int i = 0; i < MasterEngine::spectrumBins; ++i)
        {
            const float x = juce::jmap ((float) i, 0.0f, (float) (MasterEngine::spectrumBins - 1), plot.getX(), plot.getRight());
            const float db = juce::jlimit (-90.0f, 3.0f, src[(size_t) i].load());
            const float y = juce::jmap (db, -90.0f, 3.0f, plot.getBottom(), plot.getY());
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness));
    };

    drawSpectrum (processor.engine.preSpectrum, muted().withAlpha (0.55f), 1.2f);
    drawSpectrum (processor.engine.postSpectrum, cyan().withAlpha (0.85f), 1.8f);

    auto get = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    const float low = get ("lowShelf");
    const float lowMid = get ("lowMid");
    const float pres = get ("presence");
    const float air = get ("air");

    juce::Path eq;
    for (int px = 0; px < (int) plot.getWidth(); ++px)
    {
        const float norm = (float) px / juce::jmax (1.0f, plot.getWidth() - 1.0f);
        const float f = 20.0f * std::pow (1000.0f, norm);
        auto bell = [] (float freq, float centre, float width)
        {
            const float x = std::log2 (freq / centre) / width;
            return std::exp (-0.5f * x * x);
        };
        const float lowShape = 1.0f / (1.0f + std::pow (f / 130.0f, 3.0f));
        const float airShape = 1.0f / (1.0f + std::pow (8000.0f / juce::jmax (f, 20.0f), 4.0f));
        float dbv = low * lowShape
                  + lowMid * bell (f, 320.0f, 0.82f)
                  + pres * bell (f, 3200.0f, 0.78f)
                  + air * airShape;
        const float x = plot.getX() + (float) px;
        const float y = juce::jmap (juce::jlimit (-9.0f, 9.0f, dbv), -9.0f, 9.0f, plot.getCentreY() + 32.0f, plot.getCentreY() - 32.0f);
        if (px == 0) eq.startNewSubPath (x, y); else eq.lineTo (x, y);
    }
    g.setColour (violet().withAlpha (0.95f));
    g.strokePath (eq, juce::PathStrokeType (2.0f));

    g.setColour (muted());
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("PRE", (int) plot.getX(), (int) plot.getY(), 40, 14, juce::Justification::left);
    g.setColour (cyan());
    g.drawText ("POST", (int) plot.getX() + 42, (int) plot.getY(), 45, 14, juce::Justification::left);
    g.setColour (violet());
    g.drawText ("EQ", (int) plot.getX() + 90, (int) plot.getY(), 35, 14, juce::Justification::left);
}

MasterForgeAudioProcessorEditor::ModuleCard::ModuleCard (
    MasterForgeAudioProcessor& p,
    juce::String title,
    juce::String toggleId,
    juce::String helpString,
    std::initializer_list<ParamSpec> params)
    : processor (p),
      titleText (std::move (title)),
      helpText (std::move (helpString))
{
    enabled.setButtonText ({});
    enabled.setTooltip ("Enable / bypass this mastering block.");
    addAndMakeVisible (enabled);
    enabledAttachment = std::make_unique<ButtonAttachment> (processor.apvts, toggleId, enabled);

    help.setTooltip (helpText);
    help.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d2a3b));
    help.setColour (juce::TextButton::textColourOffId, text());
    addAndMakeVisible (help);

    for (const auto& spec : params)
    {
        auto c = std::make_unique<ParamControl>();
        c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
        c->slider.setColour (juce::Slider::rotarySliderFillColourId, cyan());
        c->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff263449));
        c->slider.setColour (juce::Slider::textBoxTextColourId, text());
        c->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0c1119));
        c->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        c->slider.setTextValueSuffix (spec.suffix);
        c->slider.setTooltip (spec.name + ": drag to adjust; double-click the number to type an exact value.");
        addAndMakeVisible (c->slider);

        c->label.setText (spec.name, juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setColour (juce::Label::textColourId, muted());
        c->label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        addAndMakeVisible (c->label);

        c->attachment = std::make_unique<SliderAttachment> (processor.apvts, spec.id, c->slider);
        controls.push_back (std::move (c));
    }
}

void MasterForgeAudioProcessorEditor::ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel2());
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText (titleText, 42, 10, juce::jmax (60, getWidth() - 82), 20, juce::Justification::left);
}

void MasterForgeAudioProcessorEditor::ModuleCard::resized()
{
    enabled.setBounds (10, 9, 24, 24);
    help.setBounds (getWidth() - 32, 9, 22, 22);

    if (controls.empty())
        return;

    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (34);
    const int count = (int) controls.size();
    const int cellW = juce::jmax (74, area.getWidth() / count);

    for (int i = 0; i < count; ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + i * cellW, area.getY(), cellW, area.getHeight()).reduced (3);
        controls[(size_t) i]->label.setBounds (cell.removeFromTop (18));
        controls[(size_t) i]->slider.setBounds (cell);
    }
}

MasterForgeAudioProcessorEditor::MasterForgeAudioProcessorEditor (MasterForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), spectrum (p)
{
    setResizable (true, true);
    setResizeLimits (920, 650, 1800, 1150);
    setSize (1220, 820);

    addAndMakeVisible (spectrum);

    for (int i = 0; i < (int) processor.presetNames.size(); ++i)
        presetBox.addItem (processor.presetNames[(size_t) i], i + 1);
    presetBox.setSelectedItemIndex (processor.getPresetIndex(), juce::dontSendNotification);
    presetBox.setTooltip ("Factory mastering starting points. Source level still matters; use the input coach.");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
            processor.applyPreset (idx);
    };
    addAndMakeVisible (presetBox);

    masterBypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "masterBypass", masterBypass);
    masterBypass.setTooltip ("Global transparent bypass.");
    addAndMakeVisible (masterBypass);

    auto prepMeter = [this] (juce::Label& l)
    {
        l.setColour (juce::Label::textColourId, text());
        l.setColour (juce::Label::backgroundColourId, panel());
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        addAndMakeVisible (l);
    };

    prepMeter (inputMeter); prepMeter (inputPeak); prepMeter (gainCoach);
    prepMeter (outputMeter); prepMeter (outputPeak); prepMeter (loudness);
    prepMeter (crest); prepMeter (corr); prepMeter (status);

    viewport.setViewedComponent (&moduleContent, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setColour (juce::ScrollBar::thumbColourId, line());
    addAndMakeVisible (viewport);

    addModule ("INPUT COACH", "smartGain",
               "Measures incoming RMS/peak and slowly trims toward the target RMS. This is the first density control: keep the mix healthy before compression and limiting.",
               { {"inputTrim","MANUAL TRIM"," dB"}, {"targetInput","TARGET RMS"," dB"} });

    addModule ("CLEAN EQ", "cleanEqOn",
               "Broad mastering EQ: low shelf, low-mid cleanup, presence and air. Designed for small moves; the purple line in the analyzer shows the current curve.",
               { {"lowShelf","LOW"," dB"}, {"lowMid","LOW MID"," dB"}, {"presence","PRESENCE"," dB"}, {"air","AIR"," dB"} });

    addModule ("DYNAMIC EQ", "dynamicEqOn",
               "Program-dependent low/high control. Stronger settings dynamically reduce excessive low and upper-band energy rather than applying a permanent cut.",
               { {"dynamicEq","AMOUNT",""} });

    addModule ("RESONANCE CONTROL", "resonanceOn",
               "Tames a common harsh upper-mid resonance zone with a narrow mastering-oriented reduction. Use lightly unless the mix is aggressive.",
               { {"resonance","AMOUNT",""} });

    addModule ("GLUE COMP", "glueOn",
               "Stereo bus compression with medium attack/release. Adds cohesion while preserving transient shape at moderate settings.",
               { {"glue","GLUE",""} });

    addModule ("MULTIBAND DYNAMICS", "multibandOn",
               "Three-zone level-dependent density control for lows, mids and highs. Useful for making uneven mixes feel more stable before clipping.",
               { {"multiband","AMOUNT",""} });

    addModule ("IMPACT / TRANSIENT", "impactOn",
               "Transient contrast stage. Raises short-term attack relative to the slower envelope to keep drums alive after bus compression.",
               { {"impact","IMPACT",""} });

    addModule ("ANALOG COLOR", "analogOn",
               "Low-order soft saturation inspired by tape/tube/console workflows. Adds density and harmonics before the exciter and stereo stages.",
               { {"analog","DRIVE",""} });

    addModule ("EXCITER", "exciterOn",
               "Adds controlled upper-frequency harmonic energy based on fast signal changes. Keep low for mastering.",
               { {"exciter","AMOUNT",""} });

    addModule ("BASS MONO", "bassMonoOn",
               "Centers low-frequency information below the selected crossover region to improve translation and mono compatibility.",
               { {"bassMonoHz","MONO BELOW"," Hz"} });

    addModule ("STEREO IMAGER", "imagerOn",
               "Frequency-conscious width control. Low, mid and high zones have independent width multipliers; avoid excessive low-end widening.",
               { {"widthLow","LOW","x"}, {"widthMid","MID","x"}, {"widthHigh","HIGH","x"} });

    addModule ("CLIPPER 4x", "clipperOn",
               "Four-times oversampled soft clipping placed before the final maximizer. Shaves peaks to let the limiter work less aggressively.",
               { {"clipDrive","DRIVE"," dB"}, {"clipMix","MIX",""} });

    addModule ("MAXIMIZER 4x", "limiterOn",
               "Oversampled final loudness stage with drive and output ceiling. Increase drive for loudness; watch crest factor and audible distortion.",
               { {"limiterDrive","DRIVE"," dB"}, {"ceiling","CEILING"," dB"} });

    addModule ("OUTPUT / DITHER", "ditherOn",
               "24-bit triangular dither is applied at the end of the chain. Output trim lets you level-match or leave extra delivery headroom.",
               { {"outputTrim","OUTPUT"," dB"} });

    startTimerHz (24);
}

void MasterForgeAudioProcessorEditor::addModule (
    juce::String title,
    juce::String toggleId,
    juce::String helpText,
    std::initializer_list<ParamSpec> params)
{
    auto card = std::make_unique<ModuleCard> (processor, std::move (title), std::move (toggleId), std::move (helpText), params);
    moduleContent.addAndMakeVisible (*card);
    modules.push_back (std::move (card));
}

void MasterForgeAudioProcessorEditor::timerCallback()
{
    auto fmt = [] (float v) { return juce::String (v, 1); };

    inputMeter.setText ("IN RMS  " + fmt (processor.engine.inputRmsDb.load()) + " dBFS", juce::dontSendNotification);
    inputPeak.setText ("IN PEAK  " + fmt (processor.engine.inputPeakDb.load()) + " dBFS", juce::dontSendNotification);
    gainCoach.setText ("GAIN  " + juce::String (processor.engine.smartGainAppliedDb.load(), 1) + " dB", juce::dontSendNotification);
    outputMeter.setText ("OUT RMS  " + fmt (processor.engine.outputRmsDb.load()) + " dBFS", juce::dontSendNotification);
    outputPeak.setText ("OUT PEAK  " + fmt (processor.engine.outputPeakDb.load()) + " dBFS", juce::dontSendNotification);
    loudness.setText ("LUFS EST  " + fmt (processor.engine.loudnessEstimate.load()), juce::dontSendNotification);
    crest.setText ("CREST  " + fmt (processor.engine.crestDb.load()) + " dB", juce::dontSendNotification);
    corr.setText ("CORR  " + juce::String (processor.engine.correlation.load(), 2), juce::dontSendNotification);

    const float in = processor.engine.inputRmsDb.load();
    juce::String coach = "INPUT: ";
    if (in < -22.0f) coach += "LOW - add gain";
    else if (in > -13.0f) coach += "HOT - reduce gain";
    else coach += "HEALTHY";
    status.setText (coach, juce::dontSendNotification);

    spectrum.repaint();
}

void MasterForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg());

    auto head = juce::Rectangle<int> (0, 0, getWidth(), 76).toFloat();
    juce::ColourGradient grad (juce::Colour (0xff101827), head.getX(), head.getY(),
                               juce::Colour (0xff081019), head.getRight(), head.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (head);

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("MASTERFORGE ONE", 22, 12, 330, 30, juce::Justification::left);

    g.setColour (cyan());
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText ("ALL-IN-ONE MASTERING / DENSITY / PUNCH / WIDTH / FINAL LEVEL", 24, 43, 500, 18, juce::Justification::left);
}

void MasterForgeAudioProcessorEditor::resized()
{
    const int margin = 18;
    auto area = getLocalBounds().reduced (margin);
    auto header = area.removeFromTop (48);
    header.removeFromLeft (500);

    masterBypass.setBounds (header.removeFromRight (95).reduced (4));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (360).reduced (0, 4));

    area.removeFromTop (14);

    auto topArea = area.removeFromTop ((int) juce::jlimit (190.0f, 330.0f, getHeight() * 0.31f));
    auto metersArea = topArea.removeFromRight (300);
    metersArea.removeFromLeft (12);
    spectrum.setBounds (topArea);

    const int meterH = juce::jmax (24, metersArea.getHeight() / 9);
    std::array<juce::Label*, 9> meterLabels {
        &inputMeter, &inputPeak, &gainCoach, &outputMeter, &outputPeak, &loudness, &crest, &corr, &status
    };
    for (auto* label : meterLabels)
        label->setBounds (metersArea.removeFromTop (meterH).reduced (0, 2));

    area.removeFromTop (12);
    viewport.setBounds (area);

    const int cardH = juce::jmax (118, (int) (getHeight() * 0.16f));
    const int gap = 10;
    const int contentW = juce::jmax (700, viewport.getWidth() - viewport.getScrollBarThickness() - 4);
    const int contentH = (cardH + gap) * (int) modules.size() + gap;
    moduleContent.setSize (contentW, contentH);

    int y = gap;
    for (auto& m : modules)
    {
        m->setBounds (gap, y, contentW - 2 * gap, cardH);
        y += cardH + gap;
    }
}
