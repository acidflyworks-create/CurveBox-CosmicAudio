/*
    CurveBox -- PlanetView.h
    The "living planet" of the selected module. Every knob of the module changes
    the body in real time; the live curve value drives the animation itself.
    All values are lerped so that preset changes rebuild the system smoothly.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SpaceLookAndFeel.h"

namespace cb
{

class PlanetView : public juce::Component,
                   private juce::Timer
{
public:
    explicit PlanetView (CurveBoxAudioProcessor& processorToUse)
        : processor (processorToUse)
    {
        setOpaque (false);
        startTimerHz (30);
        updateTargets (true);
    }

    ~PlanetView() override { stopTimer(); }

    void setModule (int newModule)
    {
        newModule = juce::jlimit (0, kNumModules - 1, newModule);

        if (newModule != module)
        {
            module = newModule;
            bandTextureDirty = true;
            repaint();
        }
    }

    int getModule() const noexcept { return module; }

    //==========================================================================
    void resized() override
    {
        bandTextureDirty = true;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto accent = moduleColour (module);

        paintBackdrop (g, bounds, accent);

        auto planetArea = bounds.reduced (bounds.getWidth() * 0.12f, bounds.getHeight() * 0.08f);

        switch (module)
        {
            case Volume: drawStar (g, planetArea); break;
            case Filter: drawGasGiant (g, planetArea); break;
            case Drive:  drawVolcanic (g, planetArea); break;
            case Crush:  drawPixelMoon (g, planetArea); break;
            case Pan:    drawPanPlanet (g, planetArea); break;
            case Delay:  drawMoonSystem (g, planetArea); break;
            case Grain:  drawAsteroidBelt (g, planetArea); break;
            case Reverb: drawNebula (g, planetArea); break;
            default: break;
        }

        // name plate
        g.setColour (accent.withAlpha (0.9f));
        g.setFont (spaceui::font (12.0f, true));
        g.drawText (moduleName (module).toUpperCase(),
                    getLocalBounds().removeFromTop (18).reduced (10, 0),
                    juce::Justification::centredLeft, false);

        g.setColour (spaceui::textDim);
        g.setFont (spaceui::font (11.0f));
        g.drawText (moduleAmountName (module) + " " + juce::String (anim.depth * 100.0f, 0) + "%",
                    getLocalBounds().removeFromTop (18).reduced (10, 0),
                    juce::Justification::centredRight, false);
    }

private:
    //==========================================================================
    struct Anim
    {
        float depth = 0.0f, curve = 1.0f, liveCurve = 1.0f;
        float mix = 1.0f, output = 0.0f, smooth = 5.0f, offset = 0.0f, rate = 4.0f;
        float freq = 2000.0f, res = 0.2f, mode = 0.0f;
        float driveTone = 12000.0f;
        float bits = 8.0f, downsample = 8.0f;
        float delayTime = 1.0f, delayFB = 0.35f, delayTone = 4000.0f, pingPong = 0.0f;
        float grainSize = 120.0f, grainOvl = 2.0f, grainPitch = 0.0f, grainRand = 0.0f;
        float grainSpray = 0.0f, grainTime = 200.0f, grainFB = 0.3f;
        float revSize = 0.6f, revDamp = 0.4f, revWidth = 1.0f, revPre = 20.0f;
    };

    //==========================================================================
    float param (const juce::String& moduleAndSuffix) const
    {
        return processor.getParamValue (pid (module, moduleAndSuffix));
    }

    void updateTargets (bool immediate)
    {
        auto lerpTo = [immediate] (float& destination, float target, float coeff)
        {
            destination = immediate ? target : destination + coeff * (target - destination);
        };

        constexpr float kCoeff = 0.11f; // ~300-450 ms convergence at 30 fps

        lerpTo (anim.depth, param (ids::amount) * 0.01f, kCoeff);
        lerpTo (anim.mix, processor.getParamValue (ids::mix) * 0.01f, kCoeff);
        lerpTo (anim.output, processor.getParamValue (ids::output), kCoeff);
        lerpTo (anim.smooth, processor.getParamValue (ids::smooth), kCoeff);
        lerpTo (anim.offset, processor.getParamValue (ids::offset), kCoeff);
        lerpTo (anim.rate, param (ids::rate), kCoeff);

        switch (module)
        {
            case Volume:
                break;

            case Filter:
                lerpTo (anim.freq, param (ids::filterFreq), kCoeff);
                lerpTo (anim.res, param (ids::filterRes) * 0.01f, kCoeff);
                lerpTo (anim.mode, param (ids::filterMode), kCoeff);
                break;

            case Drive:
                lerpTo (anim.driveTone, param (ids::driveTone), kCoeff);
                break;

            case Crush:
                lerpTo (anim.bits, param (ids::crushBits), kCoeff);
                lerpTo (anim.downsample, param (ids::crushSR), kCoeff);
                break;

            case Pan:
                break;

            case Delay:
            {
                lerpTo (anim.delayTime, param (ids::delayTime), kCoeff);
                lerpTo (anim.delayFB, param (ids::delayFB) * 0.01f, kCoeff);
                lerpTo (anim.delayTone, param (ids::delayTone), kCoeff);
                lerpTo (anim.pingPong, param (ids::delayPP), kCoeff);
                break;
            }

            case Grain:
                lerpTo (anim.grainSize, param (ids::grainSize), kCoeff);
                lerpTo (anim.grainOvl, param (ids::grainOvl), kCoeff);
                lerpTo (anim.grainPitch, param (ids::grainPitch), kCoeff);
                lerpTo (anim.grainRand, param (ids::grainRand), kCoeff);
                lerpTo (anim.grainSpray, param (ids::grainSpray), kCoeff);
                lerpTo (anim.grainTime, param (ids::grainTime), kCoeff);
                lerpTo (anim.grainFB, param (ids::grainFB) * 0.01f, kCoeff);
                break;

            case Reverb:
                lerpTo (anim.revSize, param (ids::reverbSize) * 0.01f, kCoeff);
                lerpTo (anim.revDamp, param (ids::reverbDamp) * 0.01f, kCoeff);
                lerpTo (anim.revWidth, param (ids::reverbWidth) * 0.01f, kCoeff);
                lerpTo (anim.revPre, param (ids::reverbPreDelay), kCoeff);
                break;

            default:
                break;
        }

        anim.liveCurve = processor.getModuleCurveValue (module);
    }

    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

        if (lastTime <= 0.0)
            lastTime = now;

        timeSeconds += juce::jlimit (0.0, 0.2, now - lastTime);
        lastTime = now;

        updateTargets (false);

        if (module == Filter && std::abs (anim.mode - lastModeDrawn) > 0.05f)
            bandTextureDirty = true;

        lastModeDrawn = anim.mode;

        repaint();
    }

    //==========================================================================
    // Background / helpers
    //==========================================================================
    void paintBackdrop (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
    {
        juce::ColourGradient bg (juce::Colour (0xff05060f), bounds.getTopLeft(),
                                 juce::Colour (0xff0c1230), bounds.getBottomRight(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 10.0f);

        // faint stars, deterministic
        for (int i = 0; i < 46; ++i)
        {
            const float x = spaceui::hashRand (7311, i) * bounds.getWidth();
            const float y = spaceui::hashRand (9931, i) * bounds.getHeight();
            const float size = 0.6f + spaceui::hashRand (1177, i) * 1.4f;
            const float twinkle = 0.35f + 0.65f * (0.5f + 0.5f * std::sin ((float) timeSeconds * 1.3f + (float) i));

            g.setColour (juce::Colours::white.withAlpha (0.25f * twinkle));
            g.fillEllipse (bounds.getX() + x, bounds.getY() + y, size, size);
        }

        g.setColour (accent.withAlpha (0.14f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    }

    juce::Colour temperatureColour (float low, float high, float position) const
    {
        return juce::Colour (low).interpolatedWith (juce::Colour (high), juce::jlimit (0.0f, 1.0f, position));
    }

    float glowRate() const
    {
        // Rate 0..11 -> orbital speed multiplier
        return 0.4f + anim.rate * 0.22f;
    }

    float phaseAngle (float speed) const
    {
        return (float) timeSeconds * speed + juce::degreesToRadians (anim.offset);
    }

    void drawTerminator (juce::Graphics& g, juce::Rectangle<float> sphere) const
    {
        const float shadow = 0.20f + 0.70f * (1.0f - juce::jlimit (0.0f, 1.0f, anim.mix));

        juce::ColourGradient term (juce::Colours::transparentBlack,
                                   sphere.getX() + sphere.getWidth() * 0.15f, sphere.getY(),
                                   juce::Colours::black.withAlpha (shadow),
                                   sphere.getRight(), sphere.getBottom(), false);
        g.setGradientFill (term);
        g.fillEllipse (sphere);
    }

    //==========================================================================
    // Volume: star with corona, breathing with the live curve value
    //==========================================================================
    void drawStar (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float baseRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.20f;

        const float breathing = 0.75f + 0.45f * anim.depth * anim.liveCurve;
        const float radius = baseRadius * (0.85f + 0.55f * anim.depth) * breathing;

        // corona
        for (int layer = 5; layer >= 1; --layer)
        {
            const float r = radius * (1.0f + 0.34f * (float) layer) * (0.9f + 0.25f * anim.liveCurve);
            const float alpha = 0.055f * (float) (6 - layer) * (0.5f + anim.depth);
            spaceui::drawGlow (g, centre, r, juce::Colour (0xffffb347), alpha);
        }

        // prominences rotating around the star
        const float speed = glowRate();
        const int prominences = 6;

        for (int i = 0; i < prominences; ++i)
        {
            const float angle = phaseAngle (speed * (0.6f + 0.1f * (float) i))
                                + juce::MathConstants<float>::twoPi * (float) i / (float) prominences;
            const auto p = centre.getPointOnCircumference (radius * 1.28f, angle);
            const float size = radius * (0.10f + 0.14f * spaceui::hashRand (3311, i)) * (0.6f + anim.liveCurve);

            spaceui::drawGlow (g, p, size * 3.0f, juce::Colour (0xffff7a1a), 0.35f * (0.4f + anim.depth));
        }

        // star body
        juce::ColourGradient body (juce::Colour (0xfffff3c4), centre.x - radius * 0.3f, centre.y - radius * 0.35f,
                                   juce::Colour (0xffff8a1f), centre.x + radius, centre.y + radius, true);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.35f * (0.4f + 0.6f * anim.liveCurve)));
        g.fillEllipse (centre.x - radius * 0.55f, centre.y - radius * 0.6f, radius * 0.75f, radius * 0.55f);

        drawTerminator (g, { centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f });
    }

    //==========================================================================
    // Filter: banded gas giant, colour temperature from the curve
    //==========================================================================
    void rebuildBandTexture (int width, int height, int mode)
    {
        width = juce::jmax (8, width);
        height = juce::jmax (8, height);

        bandTexture = juce::Image (juce::Image::ARGB, width, height, true);
        juce::Graphics g (bandTexture);

        g.setColour (juce::Colour (0xff101a2e));
        g.fillAll();

        const int bands = mode == 1 ? 22 : (mode == 2 ? 5 : 11);

        for (int i = 0; i < bands; ++i)
        {
            const float y0 = (float) height * (float) i / (float) bands;
            const float bandHeight = (float) height / (float) bands;
            const float shade = 0.35f + 0.65f * spaceui::hashRand (4409 + mode, i);

            if (mode == 2 && i < bands / 3)
                continue;

            g.setColour (juce::Colour (0xff8fb6d8).withMultipliedBrightness (shade));
            g.fillRect (0.0f, y0, (float) width, bandHeight * (mode == 1 ? 0.82f : 1.0f));
        }

        // subtle noise
        for (int i = 0; i < width * height / 90; ++i)
        {
            const float x = spaceui::hashRand (551, i) * (float) width;
            const float y = spaceui::hashRand (773, i) * (float) height;
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (x, y, 2.0f, 1.0f);
        }
    }

    void drawGasGiant (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.28f;

        const int mode = (int) std::lround (anim.mode);

        if (bandTexture.isNull() || bandTextureDirty)
        {
            rebuildBandTexture (juce::jmax (32, (int) (radius * 2.2f)),
                               juce::jmax (32, (int) (radius * 2.2f)), mode);
            bandTextureDirty = false;
            lastModeDrawn = anim.mode;
        }

        // rings (resonance -> brightness)
        const float ringBright = 0.10f + 0.55f * anim.res;

        for (int i = 0; i < 3; ++i)
        {
            const float rx = radius * (1.35f + 0.22f * (float) i);
            const float ry = rx * 0.26f;

            g.setColour (juce::Colour (0xffbcdcff).withAlpha (ringBright * (0.6f - 0.12f * (float) i)));
            g.drawEllipse (centre.x - rx, centre.y - ry, rx * 2.0f, ry * 2.0f, 1.6f);
        }

        // sphere with bands
        const auto sphere = juce::Rectangle<float> (centre.x - radius, centre.y - radius,
                                                    radius * 2.0f, radius * 2.0f);

        juce::ColourGradient tint (temperatureColour (0xffc25a3a, 0xff5fc8ff, anim.liveCurve), sphere.getTopLeft(),
                                   temperatureColour (0xff7a2a1c, 0xff2a6ea8, anim.liveCurve),
                                   sphere.getBottomRight(), false);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addEllipse (sphere);
            g.reduceClipRegion (clip);

            g.setGradientFill (tint);
            g.fillRect (sphere);
            g.setOpacity (0.85f);
            g.drawImage (bandTexture, sphere);
            g.setOpacity (1.0f);
        }

        // storm
        const float stormSize = radius * (0.12f + 0.34f * anim.res);
        g.setColour (juce::Colour (0xffd9553a).withAlpha (0.30f + 0.55f * anim.res));
        g.fillEllipse (centre.x + radius * 0.22f - stormSize, centre.y + radius * 0.18f - stormSize * 0.6f,
                       stormSize * 2.0f, stormSize * 1.2f);

        drawTerminator (g, sphere);

        // atmosphere glow
        spaceui::drawGlow (g, centre, radius * (1.5f + 0.3f * anim.liveCurve),
                           temperatureColour (0xffff6a3d, 0xff5fc8ff, anim.liveCurve), 0.10f);
    }

    //==========================================================================
    // Drive: volcanic planet with lava cracks and sparks
    //==========================================================================
    void drawVolcanic (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.27f;
        const auto sphere = juce::Rectangle<float> (centre.x - radius, centre.y - radius,
                                                    radius * 2.0f, radius * 2.0f);
        const float heat = juce::jlimit (0.0f, 1.0f, anim.depth * anim.liveCurve);

        spaceui::drawGlow (g, centre, radius * (1.25f + 0.6f * heat), juce::Colour (0xffff5a10),
                           0.06f + 0.30f * heat);

        juce::ColourGradient body (juce::Colour (0xff3a1a15), centre.x - radius * 0.4f, centre.y - radius * 0.4f,
                                   juce::Colour (0xff140a08), centre.x + radius, centre.y + radius, true);
        g.setGradientFill (body);
        g.fillEllipse (sphere);

        // lava cracks
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addEllipse (sphere);
            g.reduceClipRegion (clip);

            const int cracks = 9;

            for (int i = 0; i < cracks; ++i)
            {
                juce::Path crack;
                const float startAngle = (float) i * 2.0f * juce::MathConstants<float>::pi / (float) cracks
                                         + spaceui::hashRand (901, i);

                juce::Point<float> p (centre.x + std::cos (startAngle) * radius * 0.15f,
                                      centre.y + std::sin (startAngle) * radius * 0.15f);
                crack.startNewSubPath (p);

                for (int step = 1; step <= 4; ++step)
                {
                    const float t = (float) step / 4.0f;
                    const float wobble = (spaceui::hashRand (1201 + i, step) - 0.5f) * 0.7f;

                    p = { centre.x + std::cos (startAngle + wobble) * radius * t * 1.25f,
                          centre.y + std::sin (startAngle + wobble) * radius * t * 1.25f };
                    crack.lineTo (p);
                }

                g.setColour (juce::Colour (0xffff7326).withAlpha (0.25f + 0.75f * heat));
                g.strokePath (crack, juce::PathStrokeType (radius * (0.02f + 0.035f * heat),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            }
        }

        // sparks
        const int sparks = 14;

        for (int i = 0; i < sparks; ++i)
        {
            const float speed = 0.25f + 0.4f * spaceui::hashRand (2213, i);
            const float life = std::fmod ((float) timeSeconds * speed + spaceui::hashRand (331, i), 1.0f);
            const float angle = spaceui::hashRand (4421, i) * juce::MathConstants<float>::twoPi;
            const float distance = radius * (1.0f + life * (0.9f + heat * 0.9f));
            const auto p = centre.getPointOnCircumference (distance, angle);

            const float alpha = (1.0f - life) * (0.15f + 0.85f * heat);
            spaceui::drawGlow (g, p, radius * 0.09f * (1.0f - life * 0.5f), juce::Colour (0xffffb066), alpha);
        }

        drawTerminator (g, sphere);
    }

    //==========================================================================
    // Crush: pixelating moon
    //==========================================================================
    void drawPixelMoon (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.27f;
        const float amount = juce::jlimit (0.0f, 1.0f, anim.depth * anim.liveCurve);

        const float blockSize = juce::jmax (2.0f, 2.0f + amount * kMaxBlockSize);
        const float levels = juce::jmax (2.0f, std::round (16.0f - amount * 14.0f));

        spaceui::drawGlow (g, centre, radius * 1.4f, juce::Colour (0xff9d8bff), 0.06f + 0.18f * amount);

        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.reduceClipRegion (clip);

        const int steps = (int) std::ceil (radius * 2.0f / blockSize);

        for (int y = 0; y < steps; ++y)
        {
            for (int x = 0; x < steps; ++x)
            {
                const float px = centre.x - radius + (float) x * blockSize;
                const float py = centre.y - radius + (float) y * blockSize;

                const float dx = (px + blockSize * 0.5f - centre.x) / radius;
                const float dy = (py + blockSize * 0.5f - centre.y) / radius;
                const float shade = juce::jlimit (0.0f, 1.0f, 0.95f - 0.55f * (dx * dx + dy * dy)
                                                            + 0.25f * spaceui::hashRand (8819, y * 137 + x));

                auto colour = juce::Colour (0xffb9b2e8).withMultipliedBrightness (0.45f + 0.75f * shade);

                // colour quantisation
                auto quantise = [levels] (juce::uint8 v)
                {
                    const float f = (float) v / 255.0f;
                    return (juce::uint8) juce::jlimit (0, 255, (int) std::round (std::round (f * levels) / levels * 255.0f));
                };

                colour = juce::Colour (quantise (colour.getRed()),
                                       quantise (colour.getGreen()),
                                       quantise (colour.getBlue()));

                g.setColour (colour);
                g.fillRect (px, py, blockSize - 0.4f, blockSize - 0.4f);
            }
        }

        drawTerminator (g, { centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f });
    }

    //==========================================================================
    // Pan: planet sliding left/right, leaving a light trail
    //==========================================================================
    void drawPanPlanet (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.24f;
        const float travel = area.getWidth() * 0.30f;

        const float pos = juce::jlimit (0.0f, 1.0f, 0.5f + (anim.liveCurve - 0.5f) * anim.depth);
        const float x = centre.x + (pos - 0.5f) * 2.0f * travel;

        // trail: previous positions
        constexpr int trailSteps = 16;
        trail[(size_t) trailIndex] = x;
        trailY[(size_t) trailIndex] = centre.y;
        trailIndex = (trailIndex + 1) % trailSteps;

        for (int i = 0; i < trailSteps; ++i)
        {
            const int index = (trailIndex + i) % trailSteps;
            const float alpha = (float) i / (float) trailSteps * 0.25f * anim.depth;
            const auto p = juce::Point<float> (trail[(size_t) index], trailY[(size_t) index]);
            spaceui::drawGlow (g, p, radius * (0.5f + 0.5f * (float) i / (float) trailSteps),
                               juce::Colour (0xff4dffc3), alpha);
        }

        // orbit line
        g.setColour (juce::Colour (0xff4dffc3).withAlpha (0.18f));
        g.drawLine (centre.x - travel, centre.y, centre.x + travel, centre.y, 1.0f);
        g.drawEllipse (centre.x - travel - radius * 0.4f, centre.y - radius * 0.5f,
                       (travel + radius * 0.4f) * 2.0f, radius, 1.0f);

        const auto planetCentre = juce::Point<float> (x, centre.y);

        spaceui::drawGlow (g, planetCentre, radius * 1.8f, juce::Colour (0xff4dffc3), 0.10f);

        const auto sphere = juce::Rectangle<float> (x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        juce::ColourGradient body (juce::Colour (0xff7dffe0), x - radius * 0.4f, centre.y - radius * 0.4f,
                                   juce::Colour (0xff0f4a44), x + radius, centre.y + radius, true);
        g.setGradientFill (body);
        g.fillEllipse (sphere);

        drawTerminator (g, sphere);
    }

    //==========================================================================
    // Delay: moons on orbits, count from feedback, radius from time
    //==========================================================================
    void drawMoonSystem (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float planetRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.17f;

        const float timeBeats = 1.0f + anim.delayTime; // 0..9 -> "1/2" .. "1/16T"
        const float orbitScale = area.getWidth() * (0.13f + 0.02f * timeBeats);
        const float speed = 0.9f / juce::jmax (0.4f, timeBeats);

        // planet
        const auto sphere = juce::Rectangle<float> (centre.x - planetRadius, centre.y - planetRadius,
                                                    planetRadius * 2.0f, planetRadius * 2.0f);
        juce::ColourGradient body (juce::Colour (0xff9fc2ff), centre.x - planetRadius * 0.4f,
                                   centre.y - planetRadius * 0.5f,
                                   juce::Colour (0xff12224a), centre.x + planetRadius,
                                   centre.y + planetRadius, true);
        g.setGradientFill (body);
        g.fillEllipse (sphere);
        drawTerminator (g, sphere);

        const float moonBright = 0.25f + 0.75f * juce::jlimit (0.0f, 1.0f, anim.delayTone / 20000.0f);
        const auto moonColour = temperatureColour (0xffffc98a, 0xffbfefff, moonBright)
                                    .withMultipliedBrightness (0.6f + 0.6f * moonBright);

        const int echoes = 2 + (int) std::round (anim.delayFB * 9.0f);

        for (int i = 0; i < echoes; ++i)
        {
            const float ringRadius = orbitScale * (1.0f + 0.26f * (float) i);
            const float angle = phaseAngle (speed / (1.0f + 0.22f * (float) i)) + (float) i * 1.9f;
            const float fade = std::pow (juce::jlimit (0.05f, 0.99f, anim.delayFB), (float) (i * 0.6f));

            juce::Point<float> p;

            if (anim.pingPong > 0.5f)
            {
                const float side = std::fmod (angle, juce::MathConstants<float>::twoPi)
                                           < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
                p = { centre.x + side * ringRadius * (0.55f + 0.45f * std::abs (std::cos (angle))),
                      centre.y + std::sin (angle) * ringRadius * 0.55f };
            }
            else
            {
                p = { centre.x + std::cos (angle) * ringRadius,
                      centre.y + std::sin (angle) * ringRadius * 0.55f };
            }

            g.setColour (juce::Colour (0xff6f9bff).withAlpha (0.10f * fade));
            g.drawEllipse (centre.x - ringRadius, centre.y - ringRadius * 0.55f,
                           ringRadius * 2.0f, ringRadius * 1.1f, 1.0f);

            const float moonRadius = planetRadius * (0.16f + 0.08f * spaceui::hashRand (6611, i)) * (1.0f + 0.3f * fade);
            spaceui::drawGlow (g, p, moonRadius * 3.0f, moonColour, 0.30f * fade);
            g.setColour (moonColour.withAlpha (0.35f + 0.65f * fade));
            g.fillEllipse (p.x - moonRadius, p.y - moonRadius, moonRadius * 2.0f, moonRadius * 2.0f);
        }
    }

    //==========================================================================
    // Grain: asteroid belt / rings
    //==========================================================================
    void drawAsteroidBelt (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float planetRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.17f;

        const auto sphere = juce::Rectangle<float> (centre.x - planetRadius, centre.y - planetRadius,
                                                    planetRadius * 2.0f, planetRadius * 2.0f);
        juce::ColourGradient body (juce::Colour (0xffe0b0ff), centre.x - planetRadius * 0.4f,
                                   centre.y - planetRadius * 0.5f,
                                   juce::Colour (0xff241040), centre.x + planetRadius,
                                   centre.y + planetRadius, true);
        g.setGradientFill (body);
        g.fillEllipse (sphere);
        drawTerminator (g, sphere);

        const float beltRadius = area.getWidth() * (0.26f + 0.00022f * anim.grainTime);
        const float particleSize = 0.6f + anim.grainSize / 90.0f;
        const int count = 40 + (int) std::round (anim.grainOvl * 40.0f);

        const float direction = anim.grainPitch >= 0.0f ? 1.0f : -1.0f;
        const float speed = direction * (0.25f + std::abs (anim.grainPitch) * 0.045f);
        const float scatter = 0.05f + anim.grainSpray / 1400.0f;

        for (int i = 0; i < count; ++i)
        {
            const float baseAngle = juce::MathConstants<float>::twoPi * spaceui::hashRand (991, i);
            const float angle = baseAngle + phaseAngle (speed) * (1.0f + 0.2f * spaceui::hashRand (1123, i));

            const float radialJitter = (spaceui::hashRand (2003, i) - 0.5f) * 2.0f * scatter;
            const float radius = beltRadius * (1.0f + radialJitter);

            const auto p = juce::Point<float> (centre.x + std::cos (angle) * radius,
                                               centre.y + std::sin (angle) * radius * 0.42f);

            const float hueShift = anim.grainRand / 12.0f * (spaceui::hashRand (3011, i) - 0.5f);
            const auto colour = juce::Colour (0xffe07bff)
                                    .interpolatedWith (juce::Colour (0xff7bd7ff), juce::jlimit (0.0f, 1.0f, 0.5f + hueShift))
                                    .withMultipliedBrightness (0.7f + 0.6f * spaceui::hashRand (4051, i));

            const float size = particleSize * (0.5f + spaceui::hashRand (5077, i));

            spaceui::drawGlow (g, p, size * 3.0f, colour, 0.25f);
            g.setColour (colour);
            g.fillEllipse (p.x - size * 0.5f, p.y - size * 0.5f, size, size);

            // trails from feedback
            if (anim.grainFB > 0.05f)
            {
                for (int t = 1; t <= 3; ++t)
                {
                    const float trailAngle = angle - speed * 0.06f * (float) t;
                    const auto tp = juce::Point<float> (centre.x + std::cos (trailAngle) * radius,
                                                        centre.y + std::sin (trailAngle) * radius * 0.42f);
                    g.setColour (colour.withAlpha (0.20f * anim.grainFB / (float) t));
                    g.fillEllipse (tp.x - size * 0.35f, tp.y - size * 0.35f, size * 0.7f, size * 0.7f);
                }
            }
        }
    }

    //==========================================================================
    // Reverb: atmosphere + nebula
    //==========================================================================
    void drawNebula (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto centre = area.getCentre();
        const float planetRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.16f;

        const float gap = planetRadius * (1.0f + anim.revPre / 90.0f);
        const float atmosphere = planetRadius * (1.4f + anim.revSize * 2.4f);
        const float widthScale = 0.6f + anim.revWidth * 0.9f;
        const float darkness = juce::jlimit (0.0f, 1.0f, anim.revDamp);

        // nebula wisps
        for (int i = 0; i < 7; ++i)
        {
            const float angle = phaseAngle (0.15f + 0.05f * (float) i) + (float) i * 0.9f;
            const float distance = gap + (atmosphere - gap) * spaceui::hashRand (7717, i) * 0.9f;
            const auto p = juce::Point<float> (centre.x + std::cos (angle) * distance * widthScale * 0.5f,
                                               centre.y + std::sin (angle) * distance * 0.4f);
            const float size = atmosphere * (0.35f + 0.45f * spaceui::hashRand (8819, i));

            spaceui::drawGlow (g, p, size, juce::Colour (0xff8fd7ff)
                                              .withMultipliedBrightness (1.0f - darkness * 0.55f),
                               0.05f + 0.06f * (1.0f - darkness));
        }

        // atmospheric halo, stretched by Width, with a gap from Pre-Delay
        juce::ColourGradient halo (juce::Colour (0xff8fd7ff)
                                       .withAlpha (0.20f * (1.0f - darkness * 0.7f)),
                                   centre.x, centre.y,
                                   juce::Colour (0xff2a3f7a).withAlpha (0.0f),
                                   centre.x + atmosphere * widthScale, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (centre.x - atmosphere * widthScale, centre.y - atmosphere,
                       atmosphere * 2.0f * widthScale, atmosphere * 2.0f);

        g.setColour (juce::Colour (0xff8fd7ff).withAlpha (0.10f));
        g.drawEllipse (centre.x - gap * widthScale, centre.y - gap,
                       gap * 2.0f * widthScale, gap * 2.0f, 1.0f);

        // planet
        const auto sphere = juce::Rectangle<float> (centre.x - planetRadius, centre.y - planetRadius,
                                                    planetRadius * 2.0f, planetRadius * 2.0f);
        juce::ColourGradient body (juce::Colour (0xffb8e6ff),
                                   centre.x - planetRadius * 0.4f, centre.y - planetRadius * 0.5f,
                                   juce::Colour (0xff16213f).withMultipliedBrightness (1.0f - darkness * 0.3f),
                                   centre.x + planetRadius, centre.y + planetRadius, true);
        g.setGradientFill (body);
        g.fillEllipse (sphere);
        drawTerminator (g, sphere);
    }

    //==========================================================================
    CurveBoxAudioProcessor& processor;
    int module = Volume;

    Anim anim;
    juce::Image bandTexture;
    bool bandTextureDirty = true;
    float lastModeDrawn = -1.0f;

    double timeSeconds = 0.0;
    double lastTime = 0.0;

    static constexpr float kMaxBlockSize = 16.0f;

    std::array<float, 16> trail { };
    std::array<float, 16> trailY { };
    int trailIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlanetView)
};

} // namespace cb
