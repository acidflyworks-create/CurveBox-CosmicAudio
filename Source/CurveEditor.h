/*
    CurveBox -- CurveEditor.h
    Orbital-map style curve editor.

      * click on empty area  -> add point
      * drag                 -> move point
      * double click         -> delete point (edge points are protected)
      * right drag / alt drag on a segment -> change the tension of that segment
      * Shift                -> temporarily disable grid snapping
      * Shape menu           -> insert factory shapes
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SpaceLookAndFeel.h"

namespace cb
{

class CurveEditor : public juce::Component,
                    private juce::Timer
{
public:
    explicit CurveEditor (CurveBoxAudioProcessor& processorToUse)
        : processor (processorToUse)
    {
        setWantsKeyboardFocus (true);
        startTimerHz (30);
    }

    ~CurveEditor() override { stopTimer(); }

    void setModule (int newModule)
    {
        newModule = juce::jlimit (0, kNumModules - 1, newModule);

        if (newModule != module)
        {
            module = newModule;
            draggingPoint = -1;
            draggingTension = -1;
            repaint();
        }
    }

    int getModule() const noexcept { return module; }

    CurveShape& curve() { return processor.getCurve (module); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        const auto accent = moduleColour (module);
        const auto plot = getPlotBounds();

        paintBackdrop (g, plot, accent);
        paintGrid (g, plot, accent);
        paintCurve (g, plot, accent);
        paintPoints (g, plot, accent);
        paintPlayhead (g, plot, accent);

        // title
        g.setColour (spaceui::textDim);
        g.setFont (spaceui::font (12.0f, true));
        g.drawText (moduleName (module) + "  /  orbital map",
                    getLocalBounds().removeFromTop (22).reduced (16, 0),
                    juce::Justification::centredLeft, false);

        g.setFont (spaceui::font (11.0f));
        g.drawText ("click: add   drag: move   dbl-click: delete   alt/right-drag: curvature",
                    getLocalBounds().removeFromTop (22).reduced (16, 0),
                    juce::Justification::centredRight, false);
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();

        const auto plot = getPlotBounds();

        if (! plot.contains (e.position))
            return;

        const int hit = findPointAt (e.position);

        const bool tensionMode = e.mods.isRightButtonDown() || e.mods.isAltDown();

        if (tensionMode)
        {
            const int segment = findSegmentAt (e.position);

            if (segment >= 0)
            {
                draggingTension = segment;
                dragStartTension = curve().getTension (segment);
                dragStartY = e.position.y;
            }

            return;
        }

        if (hit >= 0)
        {
            draggingPoint = hit;
            dragOffset = toScreen (curve().getPoint (hit)) - e.position;
            return;
        }

        // empty area: insert a new point and start dragging it
        auto pos = fromScreen (e.position);
        applySnap (pos, e.mods.isShiftDown());

        const int newIndex = curve().addPoint (pos.x, pos.y);

        if (newIndex >= 0)
        {
            draggingPoint = newIndex;
            dragOffset = juce::Point<float> (0.0f, 0.0f);
            notifyChanged();
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingTension >= 0)
        {
            const float delta = (dragStartY - e.position.y) / 120.0f;
            curve().setTension (draggingTension, juce::jlimit (0.0f, 1.0f, dragStartTension + delta));
            notifyChanged();
            repaint();
            return;
        }

        if (draggingPoint < 0)
            return;

        auto pos = fromScreen (e.position + dragOffset);
        applySnap (pos, e.mods.isShiftDown());

        curve().movePoint (draggingPoint, pos.x, pos.y);
        notifyChanged();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingPoint = -1;
        draggingTension = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int hit = findPointAt (e.position);

        if (hit > 0 && hit < curve().getNumPoints() - 1)
        {
            curve().removePoint (hit);
            draggingPoint = -1;
            notifyChanged();
            repaint();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool overPoint = findPointAt (e.position) >= 0;
        setMouseCursor (overPoint ? juce::MouseCursor::PointingHandCursor
                                  : juce::MouseCursor::CrosshairCursor);
    }

    //==========================================================================
    void showShapeMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Insert shape");
        menu.addItem (1, "Flat");
        menu.addItem (2, "Saw down");
        menu.addItem (3, "Saw up");
        menu.addItem (4, "Square");
        menu.addItem (5, "Triangle");
        menu.addItem (6, "Sine");
        menu.addItem (7, "Steps");
        menu.addItem (8, "Random steps");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
                            {
                                if (result <= 0)
                                    return;

                                const auto type = (CurveShape::ShapeType) (result - 1);
                                curve().applyShape (type);
                                draggingPoint = -1;
                                notifyChanged();
                                repaint();
                            });
    }

    std::function<void (int)> onCurveChanged;

    //==========================================================================
    void resized() override { repaint(); }

private:
    //==========================================================================
    void timerCallback() override { repaint(); }

    void notifyChanged()
    {
        processor.notifyCurveEdited (module);

        if (onCurveChanged != nullptr)
            onCurveChanged (module);
    }

    juce::Rectangle<float> getPlotBounds() const
    {
        return getLocalBounds().toFloat().reduced (14.0f, 10.0f).withTrimmedTop (18.0f);
    }

    juce::Point<float> toScreen (CurveShape::Point p) const
    {
        const auto plot = getPlotBounds();
        return { plot.getX() + p.x * plot.getWidth(),
                 plot.getBottom() - juce::jlimit (0.0f, 1.0f, p.y) * plot.getHeight() };
    }

    juce::Point<float> fromScreen (juce::Point<float> pos) const
    {
        const auto plot = getPlotBounds();
        return { juce::jlimit (0.0f, 1.0f, (pos.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth())),
                 juce::jlimit (0.0f, 1.0f, (plot.getBottom() - pos.y) / juce::jmax (1.0f, plot.getHeight())) };
    }

    void applySnap (juce::Point<float>& pos, bool shiftDown) const
    {
        if (shiftDown || ! processor.isSnapEnabled())
            return;

        constexpr float gridX = 1.0f / 16.0f;
        constexpr float gridY = 1.0f / 8.0f;

        pos.x = juce::jlimit (0.0f, 1.0f, std::round (pos.x / gridX) * gridX);
        pos.y = juce::jlimit (0.0f, 1.0f, std::round (pos.y / gridY) * gridY);
    }

    int findPointAt (juce::Point<float> pos) const
    {
        const auto& shape = processor.getCurve (module);
        int best = -1;
        float bestDistance = 11.0f;

        for (int i = 0; i < shape.getNumPoints(); ++i)
        {
            const float distance = toScreen (shape.getPoint (i)).getDistanceFrom (pos);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }

        return best;
    }

    int findSegmentAt (juce::Point<float> pos) const
    {
        const auto& shape = processor.getCurve (module);
        int best = -1;
        float bestDistance = 14.0f;
        const auto phase = fromScreen (pos);

        for (int i = 0; i < shape.getNumPoints() - 1; ++i)
        {
            const auto a = shape.getPoint (i);
            const auto b = shape.getPoint (i + 1);

            if (phase.x >= a.x && phase.x <= b.x)
            {
                const float y = CurveShape::evaluate (phase.x, shape.getPoints());
                const float pixelDistance = std::abs (toScreen ({ phase.x, y }).y - pos.y);

                if (pixelDistance < bestDistance)
                {
                    bestDistance = pixelDistance;
                    best = i;
                }
            }
        }

        return best;
    }

    //==========================================================================
    void paintBackdrop (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour accent)
    {
        juce::ColourGradient bg (juce::Colour (0xff070a18), plot.getTopLeft(),
                                 juce::Colour (0xff101a3a), plot.getBottomRight(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (plot.expanded (14.0f, 10.0f), 10.0f);

        // orbital arcs
        g.setColour (accent.withAlpha (0.07f));

        for (int i = 1; i <= 3; ++i)
        {
            const float radius = plot.getHeight() * (0.30f + 0.22f * (float) i);
            g.drawEllipse (plot.getCentreX() - radius, plot.getCentreY() - radius * 0.75f,
                           radius * 2.0f, radius * 1.5f, 1.0f);
        }

        g.setColour (accent.withAlpha (0.10f));
        g.drawRoundedRectangle (plot.expanded (14.0f, 10.0f), 10.0f, 1.0f);
    }

    void paintGrid (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour accent)
    {
        constexpr int divisionsX = 16;
        constexpr int divisionsY = 8;

        for (int i = 0; i <= divisionsX; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) divisionsX;
            const bool bar = (i % 4 == 0);

            g.setColour (accent.withAlpha (bar ? 0.16f : 0.06f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        }

        for (int i = 0; i <= divisionsY; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * (float) i / (float) divisionsY;
            g.setColour (accent.withAlpha (i == divisionsY / 2 ? 0.13f : 0.06f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
    }

    /** Samples the live curve 256 times and builds the comet-tail path. */
    juce::Path buildCurvePath (juce::Rectangle<float> plot, const std::vector<CurveShape::Point>& pts) const
    {
        juce::Path path;
        constexpr int samples = 256;

        for (int i = 0; i <= samples; ++i)
        {
            const float x = (float) i / (float) samples;
            const float y = CurveShape::evaluate (x, pts);
            const juce::Point<float> p (plot.getX() + x * plot.getWidth(),
                                        plot.getBottom() - y * plot.getHeight());

            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }

        return path;
    }

    void paintCurve (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour accent)
    {
        const auto pts = processor.getCurve (module).getPoints();
        const auto path = buildCurvePath (plot, pts);

        // filled area
        juce::Path area (path);
        area.lineTo (plot.getRight(), plot.getBottom());
        area.lineTo (plot.getX(), plot.getBottom());
        area.closeSubPath();

        juce::ColourGradient fill (accent.withAlpha (0.22f), plot.getCentreX(), plot.getY(),
                                   accent.withAlpha (0.0f), plot.getCentreX(), plot.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (area);

        // glow layers
        g.setColour (accent.withAlpha (0.10f));
        g.strokePath (path, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (accent.brighter (0.35f));
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    void paintPoints (juce::Graphics& g, juce::Rectangle<float>, juce::Colour accent)
    {
        const auto& shape = processor.getCurve (module);

        for (int i = 0; i < shape.getNumPoints(); ++i)
        {
            const auto p = toScreen (shape.getPoint (i));
            const bool active = (i == draggingPoint);

            spaceui::drawGlow (g, p, active ? 14.0f : 9.0f, accent, active ? 0.6f : 0.35f);

            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.fillEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f);

            g.setColour (accent);
            g.drawEllipse (p.x - 4.6f, p.y - 4.6f, 9.2f, 9.2f, 1.2f);
        }
    }

    void paintPlayhead (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour accent)
    {
        const float phase = processor.getModulePhase (module);
        const auto& shape = processor.getCurve (module);
        const auto points = shape.getPoints();
        const float value = CurveShape::evaluate (phase, points);

        const juce::Point<float> head (plot.getX() + phase * plot.getWidth(),
                                       plot.getBottom() - value * plot.getHeight());

        // vertical playhead line
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawVerticalLine ((int) head.x, plot.getY(), plot.getBottom());

        // tail behind the head
        juce::Path tail;

        for (int i = 0; i < 24; ++i)
        {
            const float t = phase - 0.035f * (float) i / 24.0f;
            const float wrapped = t - std::floor (t);
            const float v = CurveShape::evaluate (wrapped, points);
            const juce::Point<float> p (plot.getX() + wrapped * plot.getWidth(),
                                        plot.getBottom() - v * plot.getHeight());

            if (i == 0)
                tail.startNewSubPath (head);
            tail.lineTo (p);
        }

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (tail, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        spaceui::drawGlow (g, head, 22.0f, juce::Colours::white, 0.30f);
        spaceui::drawGlow (g, head, 11.0f, accent.brighter (0.6f), 0.75f);

        g.setColour (juce::Colours::white);
        g.fillEllipse (head.x - 3.6f, head.y - 3.6f, 7.2f, 7.2f);
    }

    //==========================================================================
    CurveBoxAudioProcessor& processor;
    int module = Volume;
    int draggingPoint = -1;
    int draggingTension = -1;
    float dragStartTension = 0.5f;
    float dragStartY = 0.0f;
    juce::Point<float> dragOffset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};

} // namespace cb
