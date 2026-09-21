#pragma once
#include <JuceHeader.h>

namespace VocalForgeUI
{
    static const juce::Colour bg0       { 0xff07090d };
    static const juce::Colour bg1       { 0xff0d1118 };
    static const juce::Colour panel     { 0xff111823 };
    static const juce::Colour panel2    { 0xff161f2b };
    static const juce::Colour border    { 0xff263142 };
    static const juce::Colour text      { 0xfff4f7fb };
    static const juce::Colour muted     { 0xff8491a3 };
    static const juce::Colour cyan      { 0xff36c6ff };
    static const juce::Colour cyan2     { 0xff79e1ff };
    static const juce::Colour violet    { 0xff8b7cff };
    static const juce::Colour green     { 0xff58e3a4 };
    static const juce::Colour amber     { 0xffffc66d };
    static const juce::Colour red       { 0xffff6b7a };

    class PremiumLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        PremiumLookAndFeel()
        {
            setColour (juce::ComboBox::backgroundColourId, panel2);
            setColour (juce::ComboBox::outlineColourId, border);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, cyan2);
            setColour (juce::PopupMenu::backgroundColourId, bg1);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, panel2.brighter (0.14f));
            setColour (juce::PopupMenu::highlightedTextColourId, text);
            setColour (juce::Slider::textBoxTextColourId, text);
            setColour (juce::Slider::textBoxBackgroundColourId, bg0);
            setColour (juce::Slider::textBoxOutlineColourId, border);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, text);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float startAngle, float endAngle,
                               juce::Slider&) override
        {
            auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (9.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const float lineW = juce::jmax (2.0f, radius * 0.09f);
            const auto arcRadius = radius - lineW * 1.3f;
            const auto angle = startAngle + sliderPos * (endAngle - startAngle);

            g.setColour (juce::Colours::black.withAlpha (0.42f));
            g.fillEllipse (bounds.translated (0.0f, 3.0f));

            juce::ColourGradient face (panel2.brighter (0.08f), centre.x, bounds.getY(),
                                       bg1, centre.x, bounds.getBottom(), false);
            g.setGradientFill (face);
            g.fillEllipse (bounds);

            juce::Path bgArc;
            bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
            g.setColour (border.brighter (0.08f));
            g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
            juce::ColourGradient grad (cyan, bounds.getX(), centre.y, violet, bounds.getRight(), centre.y, false);
            g.setGradientFill (grad);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path pointer;
            const float pointerLength = radius * 0.46f;
            const float pointerThickness = juce::jmax (2.0f, radius * 0.055f);
            pointer.addRoundedRectangle (-pointerThickness * 0.5f, -radius * 0.54f,
                                         pointerThickness, pointerLength, pointerThickness * 0.5f);
            pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
            g.setColour (text.withAlpha (0.95f));
            g.fillPath (pointer);

            g.setColour (border);
            g.drawEllipse (bounds, 1.0f);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            auto fill = panel2;
            if (over) fill = fill.brighter (0.10f);
            if (down) fill = fill.brighter (0.18f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour ((over ? cyan : border).withAlpha (0.9f));
            g.drawRoundedRectangle (r, 7.0f, 1.0f);
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            auto box = juce::Rectangle<float> (2.0f, ((float) b.getHeight() - 16.0f) * 0.5f, 30.0f, 16.0f);
            const bool on = b.getToggleState();
            g.setColour (on ? cyan.withAlpha (0.24f) : bg0);
            g.fillRoundedRectangle (box, 8.0f);
            g.setColour (on ? cyan : border.brighter (0.12f));
            g.drawRoundedRectangle (box, 8.0f, 1.0f);

            auto dot = juce::Rectangle<float> (on ? box.getRight() - 13.0f : box.getX() + 3.0f,
                                               box.getY() + 3.0f, 10.0f, 10.0f);
            g.setColour (on ? cyan2 : muted);
            g.fillEllipse (dot);

            if (b.getButtonText().isNotEmpty())
            {
                g.setColour (on ? text : muted);
                g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
                g.drawText (b.getButtonText(), 39, 0, b.getWidth() - 41, b.getHeight(), juce::Justification::centredLeft);
            }

            if (over)
            {
                g.setColour (cyan.withAlpha (0.12f));
                g.drawRoundedRectangle (b.getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
            }
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox&) override
        {
            auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
            g.setColour (panel2);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (border);
            g.drawRoundedRectangle (r, 8.0f, 1.0f);

            juce::Path arrow;
            const float cx = (float) width - 17.0f;
            const float cy = (float) height * 0.5f;
            arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
            arrow.lineTo (cx, cy + 2.0f);
            arrow.lineTo (cx + 4.0f, cy - 2.0f);
            g.setColour (cyan2);
            g.strokePath (arrow, juce::PathStrokeType (1.8f));
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override
        {
            return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
        }

        juce::Font getLabelFont (juce::Label& label) override
        {
            return label.getFont();
        }
    };

    class ModuleTile : public juce::Component
    {
    public:
        ModuleTile (juce::String moduleName, juce::String helpText)
            : name (std::move (moduleName)), description (std::move (helpText)), help ("?")
        {
            power.setClickingTogglesState (true);
            power.setToggleState (true, juce::dontSendNotification);
            power.setTooltip (description);
            help.setTooltip (description);
            help.setWantsKeyboardFocus (false);

            addAndMakeVisible (power);
            addAndMakeVisible (help);
            setTooltip (description);
        }

        juce::ToggleButton& getPowerButton() { return power; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const bool on = power.getToggleState();

            g.setColour (on ? panel2 : bg1);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour ((on ? cyan : border).withAlpha (on ? 0.52f : 0.7f));
            g.drawRoundedRectangle (r, 8.0f, 1.0f);

            if (on)
            {
                auto glow = r.removeFromTop (2.0f).reduced (8.0f, 0.0f);
                g.setColour (cyan.withAlpha (0.85f));
                g.fillRoundedRectangle (glow, 1.0f);
            }

            g.setColour (on ? text : muted);
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            g.drawFittedText (name, 38, 0, getWidth() - 62, getHeight(),
                              juce::Justification::centredLeft, 1);
        }

        void resized() override
        {
            power.setBounds (6, (getHeight() - 24) / 2, 32, 24);
            help.setBounds (getWidth() - 25, (getHeight() - 22) / 2, 20, 22);
        }

    private:
        juce::String name, description;
        juce::ToggleButton power;
        juce::TextButton help;
    };
}
