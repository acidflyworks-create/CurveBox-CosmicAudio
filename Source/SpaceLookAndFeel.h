/*
    CurveBox -- SpaceLookAndFeel.h
    Custom LookAndFeel: orbit-ring rotary sliders with a satellite pointer,
    neon combo boxes and buttons. Also hosts the shared palette and helpers.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Params.h"

namespace cb
{

//==============================================================================
namespace spaceui
{
    inline const juce::Colour bgDeep      { 0xff04050c };
    inline const juce::Colour bgMid       { 0xff0a0e24 };
    inline const juce::Colour bgPurple    { 0xff170a30 };
    inline const juce::Colour panelFill   { 0x99202a55 };
    inline const juce::Colour textMain    { 0xffe3e9ff };
    inline const juce::Colour textDim     { 0xff8590b8 };
    inline const juce::Colour neonCyan    { 0xff5fd8ff };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height).withStyle (bold ? "Bold" : "Regular"));
    }

    /** Soft glowing dot used for stars and grain particles. */
    inline void drawGlow (juce::Graphics& g, juce::Point<float> centre, float radius,
                          juce::Colour colour, float alpha)
    {
        if (radius <= 0.2f || alpha <= 0.002f)
            return;

        juce::ColourGradient grad (colour.withAlpha (alpha * 0.85f), centre.x, centre.y,
                                   colour.withAlpha (0.0f), centre.x + radius, centre.y, true);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    }

    /** Cheap deterministic pseudo random number in 0..1 (no state, no locks). */
    inline float hashRand (int seed, int index)
    {
        juce::uint32 h = (juce::uint32) (seed * 374761393u + index * 668265263u);
        h = (h ^ (h >> 13)) * 1274126177u;
        h = h ^ (h >> 16);
        return (float) (h & 0xffffffu) / (float) 0x1000000u;
    }
}

//==============================================================================
class SpaceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SpaceLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, spaceui::bgDeep);
        setColour (juce::Label::textColourId, spaceui::textMain);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff16203f));
        setColour (juce::TextButton::textColourOffId, spaceui::textMain);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff101a33));
        setColour (juce::ComboBox::textColourId, spaceui::textMain);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2d3c6b));
        setColour (juce::ComboBox::arrowColourId, spaceui::neonCyan);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b1130));
        setColour (juce::PopupMenu::textColourId, spaceui::textMain);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff233a72));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Slider::rotarySliderFillColourId, spaceui::neonCyan);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff26324f));
        setColour (juce::Slider::textBoxTextColourId, spaceui::textMain);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::textColourId, spaceui::textMain);
        setColour (juce::ToggleButton::tickColourId, spaceui::neonCyan);
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff3a4460));
    }

    //==========================================================================
    juce::Font getLabelFont (juce::Label&) override       { return spaceui::font (13.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return spaceui::font (13.0f); }
    juce::Font getPopupMenuFont() override                { return spaceui::font (14.0f); }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return spaceui::font (juce::jlimit (11.0f, 16.0f, (float) buttonHeight * 0.45f), true);
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float outerRadius = size * 0.5f;
        const float ringRadius = outerRadius * 0.80f;
        const float thickness = juce::jmax (2.0f, outerRadius * 0.13f);

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto trackColour = slider.findColour (juce::Slider::rotarySliderOutlineColourId);

        // faint outer orbit + a couple of decorative orbits
        g.setColour (trackColour.withAlpha (0.35f));
        g.drawEllipse (centre.x - outerRadius, centre.y - outerRadius,
                       outerRadius * 2.0f, outerRadius * 2.0f, 1.0f);

        g.setColour (trackColour.withAlpha (0.20f));
        g.drawEllipse (centre.x - ringRadius * 0.72f, centre.y - ringRadius * 0.72f,
                       ringRadius * 1.44f, ringRadius * 1.44f, 1.0f);

        // value arc
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (trackColour.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        if (sliderPos > 0.0005f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                                    rotaryStartAngle, angle, true);

            g.setColour (accent.withAlpha (0.25f));
            g.strokePath (valueArc, juce::PathStrokeType (thickness * 2.6f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        // planet in the middle
        const float coreRadius = ringRadius * 0.52f;
        juce::ColourGradient core (accent.brighter (0.45f), centre.x - coreRadius * 0.4f,
                                   centre.y - coreRadius * 0.5f,
                                   accent.darker (0.85f), centre.x + coreRadius, centre.y + coreRadius,
                                   true);
        g.setGradientFill (core);
        g.fillEllipse (centre.x - coreRadius, centre.y - coreRadius,
                       coreRadius * 2.0f, coreRadius * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillEllipse (centre.x - coreRadius * 0.7f, centre.y - coreRadius * 0.85f,
                       coreRadius * 0.9f, coreRadius * 0.6f);

        // satellite pointer
        const auto satellite = centre.getPointOnCircumference (ringRadius, angle);
        spaceui::drawGlow (g, satellite, thickness * 3.0f, accent, 0.55f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (satellite.x - thickness * 0.55f, satellite.y - thickness * 0.55f,
                       thickness * 1.1f, thickness * 1.1f);
    }

    //==========================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                                .reduced (0.5f);

        g.setColour (juce::Colour (0xff101a33).withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        juce::Path arrow;
        const float cx = (float) width - 12.0f;
        const float cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 4.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.3f));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (7, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float radius = 5.0f;

        auto base = backgroundColour;

        if (shouldDrawButtonAsDown)
            base = base.brighter (0.22f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.12f);

        g.setGradientFill (juce::ColourGradient (base.brighter (0.08f), bounds.getX(), bounds.getY(),
                                                 base.darker (0.25f), bounds.getX(), bounds.getBottom(),
                                                 false));
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (base.brighter (0.5f).withAlpha (button.isEnabled() ? 0.5f : 0.2f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                               : juce::TextButton::textColourOffId)
                        .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));

        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3, 0),
                          juce::Justification::centred, 1, 0.85f);
    }

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        const float boxSize = juce::jmin (16.0f, (float) button.getHeight() - 4.0f);
        auto area = button.getLocalBounds().toFloat();
        auto box = juce::Rectangle<float> (area.getX() + 1.0f,
                                           area.getCentreY() - boxSize * 0.5f,
                                           boxSize, boxSize);

        g.setColour (juce::Colour (0xff101a33));
        g.fillRoundedRectangle (box, 3.0f);

        g.setColour (button.findColour (juce::ToggleButton::tickColourId)
                         .withAlpha (shouldDrawButtonAsHighlighted ? 0.9f : 0.65f));
        g.drawRoundedRectangle (box, 3.0f, 1.2f);

        if (button.getToggleState())
        {
            spaceui::drawGlow (g, box.getCentre(), boxSize * 0.9f,
                               button.findColour (juce::ToggleButton::tickColourId), 0.5f);

            g.setColour (button.findColour (juce::ToggleButton::tickColourId));
            g.fillEllipse (box.reduced (boxSize * 0.28f));
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId)
                         .withAlpha (button.isEnabled() ? 1.0f : 0.4f));
        g.setFont (spaceui::font (juce::jlimit (10.0f, 14.0f, (float) button.getHeight() * 0.5f)));
        g.drawFittedText (button.getButtonText(),
                          area.withTrimmedLeft (boxSize + 7.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.85f);
    }

    //==========================================================================
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (spaceui::font (11.0f));
        label->setJustificationType (juce::Justification::centred);
        return label;
    }
};

} // namespace cb
