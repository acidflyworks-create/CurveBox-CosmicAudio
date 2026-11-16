/*
    CurveBox -- PluginEditor.h
    Header bar (presets, shape menu, snap), solar-system module selector,
    orbital curve editor, live planet view and the parameter knobs.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SpaceLookAndFeel.h"
#include "CurveEditor.h"
#include "PlanetView.h"

//==============================================================================
/** Left-hand "solar system": one celestial body per shaper, used as tab strip. */
class SolarSystemView : public juce::Component,
                        private juce::Timer
{
public:
    explicit SolarSystemView (CurveBoxAudioProcessor& processorToUse)
        : processor (processorToUse)
    {
        startTimerHz (30);
    }

    ~SolarSystemView() override { stopTimer(); }

    std::function<void (int)> onModuleSelected;
    std::function<void (int)> onModuleToggled;

    void setSelectedModule (int m)
    {
        m = juce::jlimit (0, cb::kNumModules - 1, m);

        if (m != selected)
        {
            selected = m;
            repaint();
        }
    }

    int getSelectedModule() const noexcept { return selected; }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient bg (juce::Colour (0xcc070a18), bounds.getTopLeft(),
                                 juce::Colour (0xcc0f1633), bounds.getBottomRight(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 9.0f);

        g.setColour (cb::spaceui::textDim);
        g.setFont (cb::spaceui::font (12.0f, true));
        g.drawText ("SOLAR SYSTEM", getLocalBounds().removeFromTop (20).reduced (10, 0),
                    juce::Justification::centredLeft, false);

        const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getCentreY() + 6.0f);
        const float maxRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.44f;

        // the star in the middle (decorative)
        cb::spaceui::drawGlow (g, centre, maxRadius * 0.34f, juce::Colour (0xffffc94a), 0.20f);
        g.setColour (juce::Colour (0xffffc94a));
        g.fillEllipse (centre.x - 3.5f, centre.y - 3.5f, 7.0f, 7.0f);

        for (int m = 0; m < cb::kNumModules; ++m)
        {
            const auto body = getBodyPosition (m, centre, maxRadius);
            const float radius = getBodyRadius (m);
            const bool enabled = isModuleEnabled (m);
            const auto colour = cb::moduleColour (m);

            // orbit
            const float orbitRadius = getOrbitRadius (m, maxRadius);
            g.setColour (colour.withAlpha (selectedOrbitAlpha (m)));
            g.drawEllipse (centre.x - orbitRadius, centre.y - orbitRadius * 0.76f,
                           orbitRadius * 2.0f, orbitRadius * 1.52f, 1.0f);

            // body glow
            if (enabled || m == selected)
                cb::spaceui::drawGlow (g, body, radius * (enabled ? 3.6f : 2.2f), colour,
                                       enabled ? 0.42f : 0.18f);

            if (! enabled)
            {
                g.setColour (colour.withMultipliedBrightness (0.35f).withAlpha (0.55f));
            }
            else
            {
                juce::ColourGradient grad (colour.brighter (0.5f), body.x - radius * 0.4f,
                                           body.y - radius * 0.5f,
                                           colour.darker (0.7f), body.x + radius, body.y + radius, true);
                g.setGradientFill (grad);
            }

            g.fillEllipse (body.x - radius, body.y - radius, radius * 2.0f, radius * 2.0f);

            // selection ring
            if (m == selected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawEllipse (body.x - radius - 3.5f, body.y - radius - 3.5f,
                               (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f, 1.4f);
            }

            // label
            g.setColour (enabled ? cb::spaceui::textMain : cb::spaceui::textDim.withAlpha (0.75f));
            g.setFont (cb::spaceui::font (juce::jmax (9.0f, radius * 0.95f), m == selected));

            auto labelArea = juce::Rectangle<float> (body.x - radius * 3.0f, body.y + radius + 1.0f,
                                                     radius * 6.0f, radius * 2.0f);
            g.drawText (cb::moduleName (m), labelArea.toNearestInt(), juce::Justification::centred, false);
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getCentreY() + 6.0f);
        const float maxRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.44f;

        for (int m = 0; m < cb::kNumModules; ++m)
        {
            const auto body = getBodyPosition (m, centre, maxRadius);
            const float hitRadius = getBodyRadius (m) + 9.0f;

            if (body.getDistanceFrom (e.position) <= hitRadius)
            {
                if (e.mods.isRightButtonDown() || e.mods.isAltDown())
                {
                    if (onModuleToggled != nullptr)
                        onModuleToggled (m);
                }
                else if (onModuleSelected != nullptr)
                {
                    onModuleSelected (m);
                }

                return;
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getCentreY() + 6.0f);
        const float maxRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.44f;

        for (int m = 0; m < cb::kNumModules; ++m)
        {
            if (getBodyPosition (m, centre, maxRadius).getDistanceFrom (e.position) <= getBodyRadius (m) + 9.0f)
            {
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
                return;
            }
        }

        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

private:
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double delta = lastTime > 0.0 ? juce::jlimit (0.0, 0.2, now - lastTime) : 0.0;
        lastTime = now;

        for (int m = 0; m < cb::kNumModules; ++m)
        {
            if (isModuleEnabled (m))
                angles[(size_t) m] += (float) delta * (0.25f + 0.09f * (float) (m % 4));

            angles[(size_t) m] = std::fmod (angles[(size_t) m],
                                            juce::MathConstants<float>::twoPi);
        }

        repaint();
    }

    bool isModuleEnabled (int m) const
    {
        if (auto* p = processor.apvts.getParameter (cb::pid (m, cb::ids::enabled)))
            return p->getValue() > 0.5f;

        return false;
    }

    float getOrbitRadius (int m, float maxRadius) const
    {
        const int ring = m / 3;
        return maxRadius * (0.42f + 0.26f * (float) ring);
    }

    juce::Point<float> getBodyPosition (int m, juce::Point<float> centre, float maxRadius) const
    {
        const float orbitRadius = getOrbitRadius (m, maxRadius);
        const float baseAngle = juce::MathConstants<float>::twoPi * (float) (m % 3) / 3.0f
                                + (float) (m / 3) * 0.6f;
        const float angle = baseAngle + angles[(size_t) m];

        return { centre.x + std::cos (angle) * orbitRadius,
                 centre.y + std::sin (angle) * orbitRadius * 0.76f };
    }

    float getBodyRadius (int m) const
    {
        const float base = juce::jmin (getWidth(), getHeight()) * 0.030f;
        const float scale = m == cb::Volume ? 1.35f : (m == cb::Crush ? 0.8f : 1.0f);
        return base * scale * (m == selected ? 1.3f : 1.0f);
    }

    float selectedOrbitAlpha (int m) const
    {
        if (m == selected)
            return 0.30f;

        return isModuleEnabled (m) ? 0.12f : 0.05f;
    }

    CurveBoxAudioProcessor& processor;
    int selected = cb::Volume;
    std::array<float, cb::kNumModules> angles { };
    double lastTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SolarSystemView)
};

//==============================================================================
class CurveBoxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit CurveBoxAudioProcessorEditor (CurveBoxAudioProcessor&);
    ~CurveBoxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    struct Control
    {
        int module = -1; // -1 = global
        juce::String paramId, title;

        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::Label> label;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;

        juce::Component* widget() const
        {
            if (slider != nullptr) return slider.get();
            if (combo != nullptr)  return combo.get();
            return toggle.get();
        }
    };

    void addSliderControl (int module, const juce::String& paramId, const juce::String& title);
    void addChoiceControl (int module, const juce::String& paramId, const juce::String& title);
    void addToggleControl (int module, const juce::String& paramId, const juce::String& title);

    void buildControls();
    void layoutControls();
    void setModule (int module);

    void timerCallback() override;
    void refreshPresetUI();
    void rebuildStarfield();
    void updateHoverText();
    juce::String describeParam (const juce::String& paramId) const;

    float uiScale() const;

    //==========================================================================
    CurveBoxAudioProcessor& processor;
    cb::SpaceLookAndFeel spaceLookAndFeel;
    juce::TooltipWindow tooltipWindow { nullptr, 800 };

    juce::Label logoLabel;
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::ComboBox presetBox;
    juce::TextButton shapeButton { "Shape" };
    juce::ToggleButton snapButton { "Snap" };
    juce::ToggleButton enableButton { "Enabled" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
    juce::Label hoverLabel;

    SolarSystemView solarSystem;
    std::unique_ptr<cb::CurveEditor> curveEditor;
    std::unique_ptr<cb::PlanetView> planetView;

    std::vector<std::unique_ptr<Control>> controls;
    juce::Rectangle<int> globalControlArea, moduleControlArea;

    juce::Image starfield;
    int currentModule = cb::Volume;
    double backgroundPhase = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveBoxAudioProcessorEditor)
};
