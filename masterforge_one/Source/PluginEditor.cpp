#include "PluginEditor.h"

namespace
{
juce::Colour bg()       { return juce::Colour (0xff070b10); }
juce::Colour header()   { return juce::Colour (0xff0b1118); }
juce::Colour panel()    { return juce::Colour (0xff101821); }
juce::Colour panel2()   { return juce::Colour (0xff151f2a); }
juce::Colour panel3()   { return juce::Colour (0xff1a2632); }
juce::Colour line()     { return juce::Colour (0xff2a3948); }
juce::Colour accent()   { return juce::Colour (0xff40d8ff); }
juce::Colour violet()   { return juce::Colour (0xff9f8cff); }
juce::Colour green()    { return juce::Colour (0xff72e3a6); }
juce::Colour amber()    { return juce::Colour (0xffffc15b); }
juce::Colour red()      { return juce::Colour (0xffff6b72); }
juce::Colour text()     { return juce::Colour (0xffedf5ff); }
juce::Colour muted()    { return juce::Colour (0xff8fa0b3); }
juce::Colour dim()      { return juce::Colour (0xff526172); }

juce::String U (const char8_t* s)
{
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (s));
}

float normDb (float db)
{
    return juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
}
}

MasterForgeAudioProcessorEditor::ForgeLookAndFeel::ForgeLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panel2());
    setColour (juce::ComboBox::textColourId, text());
    setColour (juce::ComboBox::outlineColourId, line());
    setColour (juce::PopupMenu::backgroundColourId, panel2());
    setColour (juce::PopupMenu::textColourId, text());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff243648));
    setColour (juce::PopupMenu::highlightedTextColourId, text());
}

void MasterForgeAudioProcessorEditor::ForgeLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 18.0f;
    auto knob = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    knob.translate (0.0f, -3.0f);

    const float radius = knob.getWidth() * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 2.5f;

    juce::Path track;
    track.addCentredArc (knob.getCentreX(), knob.getCentreY(), arcRadius, arcRadius,
                         0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff263543));
    g.strokePath (track, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (knob.getCentreX(), knob.getCentreY(), arcRadius, arcRadius,
                            0.0f, rotaryStartAngle, angle, true);
    g.setColour (slider.isEnabled() ? accent() : dim());
    g.strokePath (valueArc, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto inner = knob.reduced (8.0f);
    juce::ColourGradient grad (juce::Colour (0xff374654), inner.getX(), inner.getY(),
                               juce::Colour (0xff111922), inner.getRight(), inner.getBottom(), false);
    grad.addColour (0.52, juce::Colour (0xff202c37));
    g.setGradientFill (grad);
    g.fillEllipse (inner);

    g.setColour (juce::Colour (0xff4a5c6b));
    g.drawEllipse (inner, 1.0f);

    const auto c = inner.getCentre();
    const float pointerLen = inner.getWidth() * 0.34f;
    const float pointerThickness = 2.2f;
    juce::Path p;
    p.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLen + 3.0f,
                           pointerThickness, pointerLen, 1.0f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (slider.isEnabled() ? text() : dim());
    g.fillPath (p);

    const float dotR = 2.3f;
    const auto dotX = c.x + std::sin (angle) * (radius - 6.0f);
    const auto dotY = c.y - std::cos (angle) * (radius - 6.0f);
    g.setColour (accent());
    g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
}

void MasterForgeAudioProcessorEditor::ForgeLookAndFeel::drawToggleButton (
    juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    auto switchArea = bounds;
    if (button.getButtonText().isNotEmpty())
        switchArea = bounds.removeFromLeft (38.0f);

    const float h = juce::jmin (20.0f, switchArea.getHeight() - 2.0f);
    const float w = juce::jmin (34.0f, switchArea.getWidth() - 2.0f);
    auto pill = juce::Rectangle<float> (w, h).withCentre (switchArea.getCentre());

    g.setColour (on ? accent().withAlpha (0.32f) : juce::Colour (0xff202d38));
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (on ? accent() : line());
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

    const float dot = h - 6.0f;
    const float dx = on ? pill.getRight() - dot - 3.0f : pill.getX() + 3.0f;
    g.setColour (on ? accent() : muted());
    g.fillEllipse (dx, pill.getY() + 3.0f, dot, dot);

    if (button.getButtonText().isNotEmpty())
    {
        g.setColour (down ? text().darker (0.2f) : (highlighted ? juce::Colours::white : text()));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft);
    }
}

void MasterForgeAudioProcessorEditor::ForgeLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
    bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    auto c = backgroundColour;
    if (highlighted) c = c.brighter (0.10f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (highlighted ? accent().withAlpha (0.65f) : line());
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void MasterForgeAudioProcessorEditor::ForgeLookAndFeel::drawComboBox (
    juce::Graphics& g, int width, int height, bool,
    int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (panel2());
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent().withAlpha (0.8f) : line());
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    juce::Path arrow;
    const float cx = width - 18.0f;
    const float cy = height * 0.5f;
    arrow.startNewSubPath (cx - 4.5f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (muted());
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font MasterForgeAudioProcessorEditor::ForgeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
}

void MasterForgeAudioProcessorEditor::AnalyzerPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel());
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    auto plot = r.reduced (18.0f, 16.0f);
    auto titleBand = plot.removeFromTop (22.0f);

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("REAL-TIME MASTER ANALYZER", titleBand.toNearestInt(), juce::Justification::centredLeft);

    g.setColour (muted());
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("PRE", (int) titleBand.getRight() - 126, (int) titleBand.getY(), 32, 18, juce::Justification::centred);
    g.setColour (accent());
    g.drawText ("POST", (int) titleBand.getRight() - 88, (int) titleBand.getY(), 40, 18, juce::Justification::centred);
    g.setColour (violet());
    g.drawText ("EQ", (int) titleBand.getRight() - 42, (int) titleBand.getY(), 30, 18, juce::Justification::centred);

    plot.removeFromTop (4.0f);

    g.setColour (line().withAlpha (0.72f));
    for (int i = 0; i <= 8; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
    for (int i = 0; i <= 5; ++i)
    {
        const float y = plot.getY() + plot.getHeight() * (float) i / 5.0f;
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    auto drawSpectrum = [&] (const auto& src, juce::Colour c, float thickness)
    {
        juce::Path p;
        for (int i = 0; i < MasterEngine::spectrumBins; ++i)
        {
            const float x = juce::jmap ((float) i, 0.0f, (float) (MasterEngine::spectrumBins - 1),
                                       plot.getX(), plot.getRight());
            const float dbv = juce::jlimit (-90.0f, 3.0f, src[(size_t) i].load());
            const float y = juce::jmap (dbv, -90.0f, 3.0f, plot.getBottom(), plot.getY());
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawSpectrum (processor.engine.preSpectrum, muted().withAlpha (0.40f), 1.0f);
    drawSpectrum (processor.engine.postSpectrum, accent().withAlpha (0.90f), 1.8f);

    auto get = [&] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    const float lowGain = get ("lowShelf");
    const float lowFreq = get ("lowShelfHz");
    const float lowMidGain = get ("lowMid");
    const float lowMidFreq = get ("lowMidHz");
    const float lowMidQ = get ("lowMidQ");
    const float presGain = get ("presence");
    const float presFreq = get ("presenceHz");
    const float presQ = get ("presenceQ");
    const float airGain = get ("air");
    const float airFreq = get ("airHz");

    auto bell = [] (float freq, float centre, float q)
    {
        const float width = juce::jmap (juce::jlimit (0.25f, 4.0f, q), 0.25f, 4.0f, 1.35f, 0.24f);
        const float x = std::log2 (freq / juce::jmax (20.0f, centre)) / width;
        return std::exp (-0.5f * x * x);
    };

    juce::Path eq;
    const int pixels = juce::jmax (2, (int) plot.getWidth());
    for (int px = 0; px < pixels; ++px)
    {
        const float n = (float) px / (float) (pixels - 1);
        const float freq = 20.0f * std::pow (1000.0f, n);
        const float lowShape = 1.0f / (1.0f + std::pow (freq / juce::jmax (30.0f, lowFreq), 3.0f));
        const float airShape = 1.0f / (1.0f + std::pow (juce::jmax (3000.0f, airFreq) / juce::jmax (freq, 20.0f), 4.0f));
        const float dbv = lowGain * lowShape
                        + lowMidGain * bell (freq, lowMidFreq, lowMidQ)
                        + presGain * bell (freq, presFreq, presQ)
                        + airGain * airShape;

        const float x = plot.getX() + n * plot.getWidth();
        const float y = juce::jmap (juce::jlimit (-9.0f, 9.0f, dbv),
                                    -9.0f, 9.0f, plot.getCentreY() + 40.0f, plot.getCentreY() - 40.0f);
        if (px == 0) eq.startNewSubPath (x, y); else eq.lineTo (x, y);
    }

    g.setColour (violet());
    g.strokePath (eq, juce::PathStrokeType (2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (dim());
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    const std::array<juce::String, 8> labels { "20", "50", "100", "250", "1k", "4k", "10k", "20k" };
    for (int i = 0; i < (int) labels.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (labels.size() - 1);
        g.drawText (labels[(size_t) i], (int) x - 16, (int) plot.getBottom() - 14, 32, 13, juce::Justification::centred);
    }
}

void MasterForgeAudioProcessorEditor::MeterPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel());
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    auto area = r.reduced (14.0f);
    auto top = area.removeFromTop (25.0f);

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("I / O | LOUDNESS", top.toNearestInt(), juce::Justification::centredLeft);

    auto meterZone = area.removeFromTop (205.0f);
    auto numbers = meterZone.removeFromBottom (32.0f);
    auto bars = meterZone.reduced (8.0f, 3.0f);

    const float inDb = processor.engine.inputPeakDb.load();
    const float outDb = processor.engine.outputPeakDb.load();
    const float inN = normDb (inDb);
    const float outN = normDb (outDb);

    auto drawBar = [&] (juce::Rectangle<float> br, float norm, juce::String label)
    {
        g.setColour (juce::Colour (0xff081018));
        g.fillRoundedRectangle (br, 4.0f);
        g.setColour (line());
        g.drawRoundedRectangle (br, 4.0f, 1.0f);

        auto fill = br.reduced (3.0f);
        const float h = fill.getHeight() * norm;
        auto active = fill.withY (fill.getBottom() - h).withHeight (h);

        juce::ColourGradient grad (accent(), active.getCentreX(), active.getBottom(),
                                   amber(), active.getCentreX(), active.getY(), false);
        grad.addColour (0.82, green());
        g.setGradientFill (grad);
        g.fillRoundedRectangle (active, 2.5f);

        g.setColour (muted());
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText (label, (int) br.getX(), (int) br.getY() - 18, (int) br.getWidth(), 15, juce::Justification::centred);
    };

    const float barW = 42.0f;
    const float gap = 36.0f;
    const float total = barW * 2.0f + gap;
    const float x0 = bars.getCentreX() - total * 0.5f;
    drawBar ({ x0, bars.getY() + 18.0f, barW, bars.getHeight() - 20.0f }, inN, "IN");
    drawBar ({ x0 + barW + gap, bars.getY() + 18.0f, barW, bars.getHeight() - 20.0f }, outN, "OUT");

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (juce::String (inDb, 1) + " dBFS",
                (int) numbers.getX(), (int) numbers.getY(), (int) numbers.getWidth() / 2, (int) numbers.getHeight(),
                juce::Justification::centred);
    g.drawText (juce::String (outDb, 1) + " dBFS",
                (int) numbers.getCentreX(), (int) numbers.getY(), (int) numbers.getWidth() / 2, (int) numbers.getHeight(),
                juce::Justification::centred);

    auto drawStat = [&] (juce::String name, juce::String value, juce::Colour c)
    {
        auto row = area.removeFromTop (34.0f);
        g.setColour (panel2());
        g.fillRoundedRectangle (row.reduced (0.0f, 2.0f), 5.0f);
        g.setColour (muted());
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (name, row.withTrimmedLeft (9.0f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (value, row.withTrimmedRight (9.0f).toNearestInt(), juce::Justification::centredRight);
    };

    drawStat ("LUFS EST", juce::String (processor.engine.loudnessEstimate.load(), 1), text());
    drawStat ("CREST", juce::String (processor.engine.crestDb.load(), 1) + " dB", text());

    const float corr = processor.engine.correlation.load();
    drawStat ("CORRELATION", juce::String (corr, 2), corr < 0.0f ? red() : green());

    drawStat ("LIMITER GR", juce::String (processor.engine.limiterReductionDb.load(), 1) + " dB",
              processor.engine.limiterReductionDb.load() > 5.0f ? amber() : accent());

    drawStat ("INPUT GAIN", juce::String (processor.engine.smartGainAppliedDb.load(), 1) + " dB", accent());

    area.removeFromTop (8.0f);
    auto coach = area.removeFromTop (58.0f);
    const float rms = processor.engine.inputRmsDb.load();
    juce::String message;
    juce::Colour coachColour = green();

    if (rms < -25.0f)
    {
        message = U(u8"Вход тихий: добавьте уровень или включите Smart Gain.");
        coachColour = amber();
    }
    else if (rms > -11.0f)
    {
        message = U(u8"Вход горячий: снизьте уровень, чтобы сохранить транзиенты.");
        coachColour = red();
    }
    else
    {
        message = U(u8"Вход в рабочей зоне. Можно мастерить без лишнего перегруза.");
    }

    g.setColour (coachColour.withAlpha (0.10f));
    g.fillRoundedRectangle (coach, 7.0f);
    g.setColour (coachColour.withAlpha (0.75f));
    g.drawRoundedRectangle (coach, 7.0f, 1.0f);
    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (10.2f)));
    g.drawFittedText (message, coach.reduced (8.0f).toNearestInt(), juce::Justification::centredLeft, 3);
}

MasterForgeAudioProcessorEditor::ModulePage::ModulePage (
    MasterForgeAudioProcessor& p,
    juce::String title,
    juce::String toggleId,
    juce::String helpString,
    std::initializer_list<ParamSpec> params)
    : processor (p),
      titleText (std::move (title)),
      helpText (std::move (helpString))
{
    enabled.setButtonText ("");
    enabled.setTooltip (U(u8"Включить или обойти этот модуль. При выключении блок не обрабатывает сигнал."));
    addAndMakeVisible (enabled);
    enabledAttachment = std::make_unique<ButtonAttachment> (processor.apvts, toggleId, enabled);

    help.setTooltip (U(u8"Нажмите, чтобы открыть русскую инструкцию по этому модулю."));
    help.setColour (juce::TextButton::buttonColourId, panel3());
    help.setColour (juce::TextButton::textColourOffId, text());
    help.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            titleText,
            helpText,
            U(u8"Закрыть"));
    };
    addAndMakeVisible (help);

    for (const auto& spec : params)
    {
        auto c = std::make_unique<ParamControl>();

        c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        c->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                       juce::MathConstants<float>::pi * 2.75f, true);
        c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 22);
        c->slider.setColour (juce::Slider::textBoxTextColourId, text());
        c->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a1016));
        c->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        c->slider.setTooltip (spec.tooltip);

        if (spec.suffix == "%")
        {
            c->slider.textFromValueFunction = [] (double v)
            {
                return juce::String (v * 100.0, 0) + " %";
            };
            c->slider.valueFromTextFunction = [] (const juce::String& s)
            {
                return s.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getDoubleValue() / 100.0;
            };
        }
        else
        {
            c->slider.setTextValueSuffix (spec.suffix);
        }

        addAndMakeVisible (c->slider);

        c->label.setText (spec.name, juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setColour (juce::Label::textColourId, muted());
        c->label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        c->label.setTooltip (spec.tooltip);
        addAndMakeVisible (c->label);

        c->attachment = std::make_unique<SliderAttachment> (processor.apvts, spec.id, c->slider);
        controls.push_back (std::move (c));
    }
}

void MasterForgeAudioProcessorEditor::ModulePage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel());
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (line());
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (titleText, 54, 10, juce::jmax (80, getWidth() - 104), 24, juce::Justification::centredLeft);

    auto info = juce::Rectangle<float> (12.0f, (float) getHeight() - 54.0f,
                                        (float) getWidth() - 24.0f, 42.0f);
    g.setColour (juce::Colour (0xff0c131b));
    g.fillRoundedRectangle (info, 7.0f);
    g.setColour (accent().withAlpha (0.18f));
    g.drawRoundedRectangle (info, 7.0f, 1.0f);
    g.setColour (muted());
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawFittedText (U(u8"ПОДСКАЗКА: ") + helpText,
                      info.reduced (10.0f, 5.0f).toNearestInt(),
                      juce::Justification::centredLeft, 2);
}

void MasterForgeAudioProcessorEditor::ModulePage::resized()
{
    enabled.setBounds (12, 11, 34, 22);
    help.setBounds (getWidth() - 38, 9, 27, 27);

    if (controls.empty())
        return;

    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (42);
    area.removeFromBottom (58);

    const int count = (int) controls.size();
    const int rows = count > 5 ? 2 : 1;
    const int cols = (count + rows - 1) / rows;
    const int cellW = juce::jmax (94, area.getWidth() / juce::jmax (1, cols));
    const int cellH = juce::jmax (112, area.getHeight() / rows);

    for (int i = 0; i < count; ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellW,
                                          area.getY() + row * cellH,
                                          cellW, cellH).reduced (4);
        controls[(size_t) i]->label.setBounds (cell.removeFromTop (19));
        controls[(size_t) i]->slider.setBounds (cell);
    }
}

MasterForgeAudioProcessorEditor::ModuleTab::ModuleTab (
    MasterForgeAudioProcessor& p,
    juce::String title,
    juce::String toggleId,
    int index,
    std::function<void(int)> selectCallback)
    : processor (p),
      titleText (std::move (title)),
      moduleIndex (index),
      onSelect (std::move (selectCallback))
{
    enabled.setButtonText ("");
    enabled.setTooltip (U(u8"Включить/выключить модуль без открытия его страницы."));
    addAndMakeVisible (enabled);
    enabledAttachment = std::make_unique<ButtonAttachment> (processor.apvts, toggleId, enabled);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MasterForgeAudioProcessorEditor::ModuleTab::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void MasterForgeAudioProcessorEditor::ModuleTab::mouseUp (const juce::MouseEvent&)
{
    if (onSelect)
        onSelect (moduleIndex);
}

void MasterForgeAudioProcessorEditor::ModuleTab::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (selected ? juce::Colour (0xff182734) : juce::Colour (0xff111922));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (selected ? accent().withAlpha (0.72f) : line());
    g.drawRoundedRectangle (r, 8.0f, selected ? 1.4f : 1.0f);

    if (selected)
    {
        auto topLine = r.withHeight (3.0f).reduced (8.0f, 0.0f);
        g.setColour (accent());
        g.fillRoundedRectangle (topLine, 1.5f);
    }

    g.setColour (selected ? text() : muted());
    g.setFont (juce::Font (juce::FontOptions (10.2f, juce::Font::bold)));
    g.drawFittedText (titleText, 36, 10, getWidth() - 42, getHeight() - 18,
                      juce::Justification::centredLeft, 2);
}

void MasterForgeAudioProcessorEditor::ModuleTab::resized()
{
    enabled.setBounds (8, getHeight() / 2 - 10, 24, 20);
}

MasterForgeAudioProcessorEditor::MasterForgeAudioProcessorEditor (MasterForgeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      analyzer (p),
      meters (p)
{
    setLookAndFeel (&forgeLookAndFeel);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (1040, 690, 1780, 1120);
    setSize (1280, 780);

    for (int i = 0; i < (int) processor.presetNames.size(); ++i)
        presetBox.addItem (processor.presetNames[(size_t) i], i + 1);

    presetBox.setSelectedItemIndex (processor.getPresetIndex(), juce::dontSendNotification);
    presetBox.setTooltip (U(u8"Готовые стартовые цепочки мастеринга. После выбора подстройте Input, Maximizer и остальные блоки под конкретный микс."));
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
            processor.applyPreset (idx);
    };
    addAndMakeVisible (presetBox);

    masterBypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "masterBypass", masterBypass);
    masterBypass.setTooltip (U(u8"Глобальный BYPASS. Сигнал проходит без обработки всей мастеринг-цепью."));
    addAndMakeVisible (masterBypass);

    addAndMakeVisible (analyzer);
    addAndMakeVisible (meters);

    moduleStripViewport.setViewedComponent (&moduleStripContent, false);
    moduleStripViewport.setScrollBarsShown (false, true, false, false);
    moduleStripViewport.setScrollBarThickness (5);
    moduleStripViewport.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff314758));
    moduleStripViewport.setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (moduleStripViewport);

    addAndMakeVisible (moduleDetail);

    addModule ("INPUT / LEVEL", "smartGain",
               U(u8"Сначала выставьте здоровый входной уровень. TARGET RMS задаёт цель для плавного Smart Gain, SPEED — скорость его реакции, RANGE — максимальную автоматическую коррекцию. Для плотного мастера не загоняйте вход в клиппинг."),
               {
                   {"inputTrim","INPUT TRIM"," dB",U(u8"Ручной входной гейн до всей цепи. Используйте для точной подстройки уровня.")},
                   {"targetInput","TARGET RMS"," dB",U(u8"Целевой средний уровень для Smart Gain. -18 dBFS — безопасная универсальная точка.")},
                   {"smartSpeed","SPEED","%",U(u8"Скорость автоматической коррекции. Меньше — плавнее и музыкальнее; больше — быстрее.")},
                   {"smartMaxGain","RANGE"," dB",U(u8"Максимальная величина, на которую Smart Gain может поднять или опустить вход.")}
               });

    addModule ("EQUALIZER 1", "cleanEqOn",
               U(u8"Широкий чистый EQ до динамической обработки. Делайте небольшие движения: обычно ±0.5–2 dB достаточно. Частоты и Q доступны отдельно, поэтому блок уже не ограничен четырьмя фиксированными полосами."),
               {
                   {"lowShelfHz","LOW FREQ"," Hz",U(u8"Частота низкой полки. Выберите область, где нужно добавить или убрать общий вес.")},
                   {"lowShelf","LOW GAIN"," dB",U(u8"Усиление/ослабление низкой полки.")},
                   {"lowMidHz","LOW-MID FREQ"," Hz",U(u8"Центр нижней середины — зона мути, коробки или тела.")},
                   {"lowMid","LOW-MID GAIN"," dB",U(u8"Усиление/ослабление нижней середины.")},
                   {"lowMidQ","LOW-MID Q","",U(u8"Ширина полосы нижней середины. Меньше Q — шире и мягче.")},
                   {"presenceHz","PRES FREQ"," Hz",U(u8"Центральная частота присутствия/атаки.")},
                   {"presence","PRES GAIN"," dB",U(u8"Усиление/ослабление присутствия.")},
                   {"presenceQ","PRES Q","",U(u8"Ширина полосы присутствия.")},
                   {"airHz","AIR FREQ"," Hz",U(u8"Частота верхней полки воздуха.")},
                   {"air","AIR GAIN"," dB",U(u8"Добавляет или убирает верхний воздух и блеск.")}
               });

    addModule ("DYNAMIC EQ", "dynamicEqOn",
               U(u8"Динамически успокаивает избыток низа и верха только когда они выпирают. THRESHOLD определяет момент срабатывания, ATTACK/RELEASE — характер движения, LOW/HIGH XOVER — области контроля."),
               {
                   {"dynamicEq","AMOUNT","%",U(u8"Общая глубина динамического контроля.")},
                   {"dynThreshold","THRESHOLD"," dB",U(u8"Порог, выше которого динамический EQ начинает сильнее подавлять проблемную энергию.")},
                   {"dynAttack","ATTACK"," ms",U(u8"Как быстро модуль реагирует на всплески.")},
                   {"dynRelease","RELEASE"," ms",U(u8"Как быстро контроль отпускает после всплеска.")},
                   {"dynLowHz","LOW XOVER"," Hz",U(u8"Граница низкочастотной динамической зоны.")},
                   {"dynHighHz","HIGH XOVER"," Hz",U(u8"Граница верхней динамической зоны.")}
               });

    addModule ("STABILIZER", "resonanceOn",
               U(u8"Узкий резонанс-контроль для неприятной середины/верхней середины. Найдите проблемную частоту, настройте Q и добавляйте AMOUNT до исчезновения резкости без потери живости."),
               {
                   {"resonance","AMOUNT","%",U(u8"Глубина подавления выбранной резонансной зоны.")},
                   {"resonanceHz","FREQUENCY"," Hz",U(u8"Центральная частота проблемного резонанса.")},
                   {"resonanceQ","Q","",U(u8"Ширина подавления. Большой Q — узкая точечная коррекция.")}
               });

    addModule ("VINTAGE COMP", "glueOn",
               U(u8"Стерео bus-компрессор для склейки. Смотрите на транзиенты: слишком быстрый ATTACK съедает удар. MIX даёт параллельную компрессию, MAKEUP возвращает уровень без изменения порога."),
               {
                   {"glue","AMOUNT","%",U(u8"Общая интенсивность glue-обработки.")},
                   {"glueThreshold","THRESHOLD"," dB",U(u8"Порог компрессии.")},
                   {"glueRatio","RATIO",":1",U(u8"Степень компрессии после пересечения порога.")},
                   {"glueAttack","ATTACK"," ms",U(u8"Время атаки. Для ударного boom bap обычно полезна более медленная атака.")},
                   {"glueRelease","RELEASE"," ms",U(u8"Время восстановления после компрессии.")},
                   {"glueMakeup","MAKEUP"," dB",U(u8"Компенсационный уровень после компрессора.")},
                   {"glueMix","MIX","%",U(u8"Параллельное смешивание обработанного и исходного сигнала.")}
               });

    addModule ("MULTIBAND", "multibandOn",
               U(u8"Трёхполосная плотность: LOW/MID/HIGH обрабатываются отдельно. XOVER задают границы, а индивидуальные AMOUNT позволяют удержать бас, середину и верх без одинакового давления на весь микс."),
               {
                   {"multiband","GLOBAL","%",U(u8"Общая сила многополосной динамики.")},
                   {"mbLowHz","LOW XOVER"," Hz",U(u8"Граница низкой полосы.")},
                   {"mbHighHz","HIGH XOVER"," Hz",U(u8"Граница верхней полосы.")},
                   {"mbLowAmount","LOW","%",U(u8"Плотность низкой полосы.")},
                   {"mbMidAmount","MID","%",U(u8"Плотность средней полосы.")},
                   {"mbHighAmount","HIGH","%",U(u8"Плотность верхней полосы.")}
               });

    addModule ("IMPACT", "impactOn",
               U(u8"Возвращает атаку после компрессии и делает ударные выразительнее. SPEED определяет скорость огибающей, MIX — сколько обработанного транзиентного сигнала подмешивается."),
               {
                   {"impact","PUNCH","%",U(u8"Сила транзиентного усиления.")},
                   {"impactSpeed","SPEED","%",U(u8"Скорость детектора транзиентов.")},
                   {"impactMix","MIX","%",U(u8"Баланс обработанного и исходного сигнала.")}
               });

    addModule ("SATURATION", "analogOn",
               U(u8"Мягкая гармоническая сатурация для плотности. DRIVE отвечает за гармоники, TONE — за яркость окраса, MIX позволяет оставить атаку исходника. На мастере обычно лучше умеренные значения."),
               {
                   {"analog","DRIVE","%",U(u8"Количество нелинейной гармонической окраски.")},
                   {"analogTone","TONE","%",U(u8"Тон сатурации: левее темнее, правее ярче.")},
                   {"analogMix","MIX","%",U(u8"Параллельное смешивание сатурации.")}
               });

    addModule ("EXCITER", "exciterOn",
               U(u8"Добавляет контролируемые верхние гармоники, а не просто поднимает EQ. FREQ задаёт область, AMOUNT — количество гармоник, MIX — итоговую долю эффекта."),
               {
                   {"exciter","AMOUNT","%",U(u8"Интенсивность создаваемых гармоник.")},
                   {"exciterHz","FREQUENCY"," Hz",U(u8"Ниже этой области exciter практически не вмешивается.")},
                   {"exciterMix","MIX","%",U(u8"Количество эффекта в итоговом сигнале.")}
               });

    addModule ("LOW END FOCUS", "bassMonoOn",
               U(u8"Собирает суб и низ в центр для стабильного перевода на разные системы. FREQUENCY задаёт границу, AMOUNT — степень моно-совместимости. Верх и середина остаются стерео."),
               {
                   {"bassMonoHz","MONO BELOW"," Hz",U(u8"Частоты ниже этой точки постепенно центрируются.")},
                   {"bassMonoAmount","AMOUNT","%",U(u8"Степень центровки низких частот.")}
               });

    addModule ("IMAGER", "imagerOn",
               U(u8"Трёхполосная ширина с защитой по корреляции. Не расширяйте суб без необходимости. SAFETY автоматически уменьшает чрезмерное расширение при ухудшении фазовой корреляции."),
               {
                   {"widthLow","LOW WIDTH","%",U(u8"Ширина низкой полосы: 100% — исходная, меньше — уже.")},
                   {"widthMid","MID WIDTH","%",U(u8"Ширина середины.")},
                   {"widthHigh","HIGH WIDTH","%",U(u8"Ширина верхней полосы.")},
                   {"imagerLowHz","LOW XOVER"," Hz",U(u8"Граница низкой полосы имейджера.")},
                   {"imagerHighHz","HIGH XOVER"," Hz",U(u8"Граница верхней полосы имейджера.")},
                   {"imagerSafety","SAFETY","%",U(u8"Насколько сильно защита по корреляции ограничивает рискованное расширение.")}
               });

    addModule ("CLIPPER 4x", "clipperOn",
               U(u8"Четырёхкратный oversampling уменьшает алиасинг. DRIVE аккуратно срезает короткие пики перед лимитером, CEILING задаёт рабочую границу, SHAPE меняет мягкость колена, MIX позволяет ослабить эффект."),
               {
                   {"clipDrive","DRIVE"," dB",U(u8"Предусиление перед клиппером.")},
                   {"clipCeiling","CEILING"," dB",U(u8"Уровень мягкого ограничения клиппера.")},
                   {"clipShape","SHAPE","%",U(u8"Мягкость/жёсткость формы клиппинга.")},
                   {"clipMix","MIX","%",U(u8"Доля клиппированного сигнала.")}
               });

    addModule ("MAXIMIZER 4x", "limiterOn",
               U(u8"Финальный stereo-linked лимитер после 4x oversampling. DRIVE определяет громкость, CEILING — выходной потолок, RELEASE — скорость возврата усиления. Следите за LIMITER GR справа: большие значения могут съесть панч."),
               {
                   {"limiterDrive","DRIVE"," dB",U(u8"Входной драйв финального лимитера — главный регулятор итоговой громкости.")},
                   {"ceiling","CEILING"," dB",U(u8"Жёсткий выходной потолок после лимитера.")},
                   {"limiterRelease","RELEASE"," ms",U(u8"Скорость восстановления лимитера после пиков.")}
               });

    addModule ("OUTPUT", "ditherOn",
               U(u8"Финальный уровень и общий Dry/Wet. OUTPUT TRIM используется для точного level-match. DITHER добавляется в самом конце и полезен при финальном 24-bit экспорте."),
               {
                   {"outputTrim","OUTPUT TRIM"," dB",U(u8"Финальная подстройка уровня после лимитера.")},
                   {"dryWet","MASTER MIX","%",U(u8"Глобальный баланс между исходным сигналом и обработанной цепью до финального clip/limit.")}
               });

    selectModule (0);
    startTimerHz (30);
}

MasterForgeAudioProcessorEditor::~MasterForgeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MasterForgeAudioProcessorEditor::addModule (
    juce::String title,
    juce::String toggleId,
    juce::String helpText,
    std::initializer_list<ParamSpec> params)
{
    const int index = (int) pages.size();

    auto page = std::make_unique<ModulePage> (processor, title, toggleId, helpText, params);
    page->setVisible (false);
    moduleDetail.addAndMakeVisible (*page);

    auto tab = std::make_unique<ModuleTab> (
        processor, title, toggleId, index,
        [this] (int i) { selectModule (i); });
    moduleStripContent.addAndMakeVisible (*tab);

    pages.push_back (std::move (page));
    tabs.push_back (std::move (tab));
}

void MasterForgeAudioProcessorEditor::selectModule (int index)
{
    if (pages.empty())
        return;

    selectedModule = juce::jlimit (0, (int) pages.size() - 1, index);

    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == selectedModule);
        tabs[(size_t) i]->setSelected (i == selectedModule);
    }

    resized();
}

void MasterForgeAudioProcessorEditor::timerCallback()
{
    analyzer.repaint();
    meters.repaint();
}

void MasterForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg());

    auto bounds = getLocalBounds().toFloat();
    auto head = bounds.removeFromTop (66.0f);

    g.setColour (header());
    g.fillRect (head);
    g.setColour (line());
    g.drawHorizontalLine ((int) head.getBottom() - 1, head.getX(), head.getRight());

    g.setColour (text());
    g.setFont (juce::Font (juce::FontOptions (23.0f, juce::Font::bold)));
    g.drawText ("MASTERFORGE ONE", 22, 12, 330, 25, juce::Justification::centredLeft);

    g.setColour (accent());
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText ("PRO MASTERING SUITE | 8x FINAL STAGE | SMOOTH DSP",
                23, 39, 430, 16, juce::Justification::centredLeft);

    auto rack = juce::Rectangle<float> (0.0f, 66.0f, (float) getWidth(), 86.0f);
    g.setColour (juce::Colour (0xff0a1016));
    g.fillRect (rack);
    g.setColour (line().withAlpha (0.7f));
    g.drawHorizontalLine ((int) rack.getBottom() - 1, rack.getX(), rack.getRight());
}

void MasterForgeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto headerArea = bounds.removeFromTop (66);
    auto headerRight = headerArea.removeFromRight (juce::jmin (590, getWidth() / 2));
    masterBypass.setBounds (headerRight.removeFromRight (105).reduced (8, 17));
    presetBox.setBounds (headerRight.reduced (6, 14));

    auto stripArea = bounds.removeFromTop (86).reduced (12, 8);
    moduleStripViewport.setBounds (stripArea);

    const int tabW = 142;
    const int tabH = 66;
    const int gap = 6;
    const int contentW = juce::jmax (stripArea.getWidth(), (int) tabs.size() * (tabW + gap));
    moduleStripContent.setSize (contentW, tabH + 4);

    for (int i = 0; i < (int) tabs.size(); ++i)
        tabs[(size_t) i]->setBounds (i * (tabW + gap), 0, tabW, tabH);

    auto work = bounds.reduced (12, 10);
    auto meterArea = work.removeFromRight (juce::jlimit (218, 265, getWidth() / 5));
    work.removeFromRight (10);

    meters.setBounds (meterArea);

    auto analyzerArea = work.removeFromTop (juce::jlimit (215, 270, getHeight() / 3));
    analyzer.setBounds (analyzerArea);

    work.removeFromTop (10);
    moduleDetail.setBounds (work);

    for (auto& page : pages)
        page->setBounds (moduleDetail.getLocalBounds());
}
