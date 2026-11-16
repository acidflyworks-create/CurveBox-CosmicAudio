/*
    CurveBox -- Presets.h
    40 factory presets: name + parameter values (in real, user facing units) +
    one curve per module. Loading always resets everything to the parameter
    defaults first, then applies the preset, so nothing leaks between presets.
*/

#pragma once

#include "Curve.h"
#include "Params.h"

#include <vector>
#include <utility>

namespace cb
{

struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> params; // parameter id -> real value
    std::array<std::vector<CurveShape::Point>, kNumModules> curves;

    Preset() { resetCurves(); }

    explicit Preset (const juce::String& presetName) : name (presetName) { resetCurves(); }

    void resetCurves()
    {
        for (auto& c : curves)
            c = CurveShape::makeDefaultPoints();
    }
};

//==============================================================================
// Curve helpers
//==============================================================================
namespace curves
{
    using P = CurveShape::Point;
    using Points = std::vector<P>;

    inline Points flat (float y = 1.0f)
    {
        return { { 0.0f, y, 0.5f }, { 1.0f, y, 0.5f } };
    }

    inline Points ramp (float y0, float y1, float tension = 0.5f)
    {
        return { { 0.0f, y0, tension }, { 1.0f, y1, 0.5f } };
    }

    /** Fast rise at the start of the cycle, then a long recovery: classic pump. */
    inline Points pump (float floorValue = 0.03f, float attack = 0.16f, float tension = 0.3f)
    {
        return { { 0.0f, floorValue, tension },
                 { attack, 1.0f, 0.5f },
                 { 1.0f, 1.0f, 0.5f } };
    }

    inline Points gate (float duty, float high = 1.0f, float low = 0.0f)
    {
        duty = juce::jlimit (0.02f, 0.98f, duty);
        return { { 0.0f, high, 0.5f },
                 { duty - 0.0005f, high, 0.5f },
                 { duty, low, 0.5f },
                 { 1.0f, low, 0.5f } };
    }

    inline Points steps (std::initializer_list<float> values)
    {
        Points pts;
        const int n = (int) values.size();
        int i = 0;

        for (float v : values)
        {
            const float x0 = (float) i / (float) n;
            const float x1 = (float) (i + 1) / (float) n - 0.0005f;
            pts.push_back ({ x0, juce::jlimit (0.0f, 1.0f, v), 0.5f });
            pts.push_back ({ x1, juce::jlimit (0.0f, 1.0f, v), 0.5f });
            ++i;
        }

        pts.front().x = 0.0f;
        pts.back().x = 1.0f;
        return pts;
    }

    inline Points sine (int steps = 16, float low = 0.0f, float high = 1.0f)
    {
        Points pts;

        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const float s = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * x);
            pts.push_back ({ x, low + (high - low) * s, 0.5f });
        }

        return pts;
    }

    inline Points triangle (float low = 0.0f, float high = 1.0f)
    {
        return { { 0.0f, low, 0.5f }, { 0.5f, high, 0.5f }, { 1.0f, low, 0.5f } };
    }

    /** Slow ramp up that snaps back at the cycle end (riser). */
    inline Points riser (float low = 0.05f, float tension = 0.7f)
    {
        return { { 0.0f, low, tension }, { 0.97f, 1.0f, 0.5f }, { 1.0f, low, 0.5f } };
    }

    /** Saw down with a curved, exponential fall (ducking / sweep down). */
    inline Points fall (float tension = 0.35f)
    {
        return { { 0.0f, 1.0f, tension }, { 1.0f, 0.0f, 0.5f } };
    }
}

//==============================================================================
// Builder helpers
//==============================================================================
inline void setP (Preset& p, const juce::String& id, float value)
{
    p.params.emplace_back (id, value);
}

/** Enables a module and sets amount, rate and curve in one call. */
inline void addLayer (Preset& p, int module, float amount, int rateIndex,
                      std::vector<CurveShape::Point> points)
{
    setP (p, pid (module, ids::enabled), 1.0f);
    setP (p, pid (module, ids::amount), amount);
    setP (p, pid (module, ids::rate), (float) rateIndex);
    p.curves[(size_t) module] = std::move (points);
}

//==============================================================================
// Factory presets
//==============================================================================
inline std::vector<Preset> buildFactoryPresets()
{
    using namespace curves;

    std::vector<Preset> list;
    list.reserve (40);

    auto next = [] (const juce::String& name)
    {
        return Preset { name };
    };

    //--------------------------------------------------------------------------
    // 01-10 Volume
    //--------------------------------------------------------------------------
    {
        auto p = next ("01 Big Bang");
        list.push_back (std::move (p));
    }
    {
        auto p = next ("02 Pulsar Pump");
        addLayer (p, Volume, 100.0f, 4, pump (0.04f, 0.18f, 0.32f));
        setP (p, ids::smooth, 4.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("03 Supernova Pump");
        addLayer (p, Volume, 100.0f, 4, pump (0.0f, 0.28f, 0.18f));
        setP (p, ids::smooth, 1.5f);
        setP (p, ids::output, 2.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("04 Orbit Gate");
        addLayer (p, Volume, 100.0f, 5, gate (0.5f));
        setP (p, ids::smooth, 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("05 Warp Drive");
        addLayer (p, Volume, 100.0f, 6, gate (0.62f, 1.0f, 0.05f));
        setP (p, ids::smooth, 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("06 Zero Gravity");
        addLayer (p, Volume, 90.0f, 2, ramp (0.05f, 1.0f, 0.65f));
        setP (p, ids::smooth, 12.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("07 Moon Tides");
        addLayer (p, Volume, 45.0f, 3, sine (16, 0.25f, 1.0f));
        setP (p, ids::smooth, 20.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("08 Stellar Wind");
        addLayer (p, Volume, 80.0f, 0, riser (0.08f, 0.75f));
        setP (p, ids::smooth, 25.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("09 Dark Matter Gate");
        addLayer (p, Volume, 100.0f, 7, gate (0.42f, 1.0f, 0.0f));
        setP (p, ids::smooth, 0.5f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("10 Escape Velocity");
        addLayer (p, Volume, 100.0f, 11, steps ({ 1.0f, 0.35f, 0.7f, 0.2f, 0.95f, 0.45f, 0.85f, 0.3f }));
        setP (p, ids::smooth, 0.8f);
        list.push_back (std::move (p));
    }

    //--------------------------------------------------------------------------
    // 11-17 Filter
    //--------------------------------------------------------------------------
    {
        auto p = next ("11 Gravity Wobble");
        addLayer (p, Filter, 85.0f, 5, sine (16, 0.1f, 1.0f));
        setP (p, pid (Filter, ids::filterMode), 0.0f);
        setP (p, pid (Filter, ids::filterFreq), 900.0f);
        setP (p, pid (Filter, ids::filterRes), 68.0f);
        setP (p, ids::smooth, 2.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("12 Solar Flare Sweep");
        addLayer (p, Filter, 100.0f, 2, ramp (0.0f, 1.0f, 0.35f));
        setP (p, pid (Filter, ids::filterMode), 0.0f);
        setP (p, pid (Filter, ids::filterFreq), 7000.0f);
        setP (p, pid (Filter, ids::filterRes), 30.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("13 Red Shift");
        addLayer (p, Filter, 90.0f, 2, fall (0.4f));
        setP (p, pid (Filter, ids::filterMode), 0.0f);
        setP (p, pid (Filter, ids::filterFreq), 5000.0f);
        setP (p, pid (Filter, ids::filterRes), 25.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("14 Blue Shift");
        addLayer (p, Filter, 100.0f, 1, riser (0.0f, 0.55f));
        setP (p, pid (Filter, ids::filterMode), 1.0f); // high pass riser
        setP (p, pid (Filter, ids::filterFreq), 900.0f);
        setP (p, pid (Filter, ids::filterRes), 45.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("15 Acid Orbit");
        addLayer (p, Filter, 100.0f, 6, steps ({ 1.0f, 0.3f, 0.8f, 0.45f, 0.95f, 0.2f, 0.7f, 0.55f }));
        setP (p, pid (Filter, ids::filterMode), 2.0f); // band pass
        setP (p, pid (Filter, ids::filterFreq), 1600.0f);
        setP (p, pid (Filter, ids::filterRes), 88.0f);
        setP (p, ids::smooth, 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("16 Comet Talk");
        addLayer (p, Filter, 100.0f, 4, steps ({ 0.9f, 0.2f, 0.6f, 0.35f, 0.8f, 0.15f, 0.5f, 0.25f }));
        setP (p, pid (Filter, ids::filterMode), 2.0f);
        setP (p, pid (Filter, ids::filterFreq), 1100.0f);
        setP (p, pid (Filter, ids::filterRes), 72.0f);
        setP (p, ids::smooth, 3.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("17 Nebula Drift");
        addLayer (p, Filter, 40.0f, 1, sine (8, 0.35f, 1.0f));
        setP (p, pid (Filter, ids::filterMode), 0.0f);
        setP (p, pid (Filter, ids::filterFreq), 3500.0f);
        setP (p, pid (Filter, ids::filterRes), 22.0f);
        setP (p, ids::smooth, 20.0f);
        list.push_back (std::move (p));
    }

    //--------------------------------------------------------------------------
    // 18-23 Drive and Crush
    //--------------------------------------------------------------------------
    {
        auto p = next ("18 Solar Burn");
        addLayer (p, Drive, 65.0f, 4, flat (0.85f));
        setP (p, pid (Drive, ids::driveTone), 8000.0f);
        setP (p, ids::output, -3.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("19 Meteor Shower");
        addLayer (p, Drive, 100.0f, 6, steps ({ 1.0f, 0.15f, 0.65f, 0.25f, 0.9f, 0.1f, 0.5f, 0.2f }));
        setP (p, pid (Drive, ids::driveTone), 12000.0f);
        setP (p, ids::smooth, 1.5f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("20 Iron Core");
        addLayer (p, Crush, 80.0f, 5, ramp (0.45f, 1.0f, 0.6f));
        setP (p, pid (Crush, ids::crushBits), 6.0f);
        setP (p, pid (Crush, ids::crushSR), 4.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("21 Static Void");
        addLayer (p, Crush, 100.0f, 2, flat (1.0f));
        setP (p, pid (Crush, ids::crushBits), 3.0f);
        setP (p, pid (Crush, ids::crushSR), 24.0f);
        setP (p, ids::output, -2.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("22 Pulsar Static");
        addLayer (p, Drive, 70.0f, 6, flat (1.0f));
        addLayer (p, Crush, 60.0f, 11, gate (0.5f, 1.0f, 0.1f));
        setP (p, pid (Crush, ids::crushBits), 8.0f);
        setP (p, pid (Crush, ids::crushSR), 6.0f);
        setP (p, ids::output, -3.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("23 Quasar Fuzz");
        addLayer (p, Drive, 100.0f, 2, ramp (0.1f, 1.0f, 0.4f));
        setP (p, pid (Drive, ids::driveTone), 6000.0f);
        setP (p, ids::output, -4.0f);
        list.push_back (std::move (p));
    }

    //--------------------------------------------------------------------------
    // 24-27 Pan
    //--------------------------------------------------------------------------
    {
        auto p = next ("24 Binary Star");
        addLayer (p, Pan, 100.0f, 4, sine (16));
        setP (p, ids::smooth, 2.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("25 Orbital Sway");
        addLayer (p, Pan, 80.0f, 3, triangle ());
        setP (p, ids::smooth, 6.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("26 Asteroid Tumble");
        addLayer (p, Pan, 60.0f, 5, steps ({ 1.0f, 0.2f, 0.85f, 0.4f, 0.95f, 0.15f, 0.7f, 0.3f }));
        setP (p, ids::smooth, 1.5f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("27 Stardust Shimmer");
        addLayer (p, Pan, 45.0f, 6, sine (24, 0.2f, 0.8f));
        setP (p, ids::smooth, 1.0f);
        list.push_back (std::move (p));
    }

    //--------------------------------------------------------------------------
    // 28-30 Combinations
    //--------------------------------------------------------------------------
    {
        auto p = next ("28 Cosmic Storm");
        addLayer (p, Filter, 70.0f, 5, sine (16, 0.15f, 1.0f));
        setP (p, pid (Filter, ids::filterFreq), 2500.0f);
        setP (p, pid (Filter, ids::filterRes), 55.0f);
        addLayer (p, Drive, 50.0f, 4, flat (0.8f));
        addLayer (p, Pan, 60.0f, 4, sine (16));
        setP (p, ids::output, -3.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("29 Galactic Pump");
        addLayer (p, Volume, 85.0f, 4, pump (0.05f, 0.2f));
        addLayer (p, Reverb, 35.0f, 4, flat (1.0f));
        setP (p, pid (Reverb, ids::reverbSize), 70.0f);
        setP (p, pid (Reverb, ids::reverbDamp), 35.0f);
        setP (p, pid (Reverb, ids::reverbPreDelay), 20.0f);
        setP (p, ids::smooth, 4.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("30 Gravitational Collapse");
        addLayer (p, Filter, 90.0f, 2, fall (0.35f));
        setP (p, pid (Filter, ids::filterFreq), 4000.0f);
        setP (p, pid (Filter, ids::filterRes), 40.0f);
        addLayer (p, Volume, 70.0f, 2, ramp (0.4f, 1.0f, 0.5f));
        addLayer (p, Delay, 40.0f, 4, p.curves[(size_t) Volume]);
        setP (p, pid (Delay, ids::delayTime), 1.0f);
        setP (p, pid (Delay, ids::delayFB), 45.0f);
        list.push_back (std::move (p));
    }

    //--------------------------------------------------------------------------
    // 31-40 Delay / Grain / Reverb
    //--------------------------------------------------------------------------
    {
        auto p = next ("31 Deep Space Echo");
        addLayer (p, Delay, 60.0f, 4, flat (1.0f));
        setP (p, pid (Delay, ids::delayTime), 1.0f);
        setP (p, pid (Delay, ids::delayFB), 55.0f);
        setP (p, pid (Delay, ids::delayTone), 3000.0f);
        setP (p, pid (Delay, ids::delayPP), 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("32 Satellite Ping");
        addLayer (p, Delay, 70.0f, 5, flat (1.0f));
        setP (p, pid (Delay, ids::delayTime), 5.0f); // 1/8.
        setP (p, pid (Delay, ids::delayFB), 62.0f);
        setP (p, pid (Delay, ids::delayTone), 9000.0f);
        setP (p, pid (Delay, ids::delayPP), 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("33 Black Hole Gate");
        addLayer (p, Reverb, 80.0f, 4, gate (0.5f));
        setP (p, pid (Reverb, ids::reverbSize), 85.0f);
        setP (p, pid (Reverb, ids::reverbDamp), 25.0f);
        setP (p, pid (Reverb, ids::reverbPreDelay), 30.0f);
        setP (p, ids::smooth, 4.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("34 Ducked Nebula");
        addLayer (p, Reverb, 60.0f, 4, flat (1.0f));
        setP (p, pid (Reverb, ids::reverbSize), 75.0f);
        setP (p, pid (Reverb, ids::reverbDamp), 45.0f);
        addLayer (p, Volume, 70.0f, 4, pump (0.1f, 0.22f));
        setP (p, ids::smooth, 5.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("35 Starlight Echo");
        addLayer (p, Delay, 55.0f, 4, flat (1.0f));
        setP (p, pid (Delay, ids::delayTime), 2.0f); // 1/4.
        setP (p, pid (Delay, ids::delayFB), 45.0f);
        setP (p, pid (Delay, ids::delayTone), 6000.0f);
        addLayer (p, Filter, 50.0f, 2, ramp (0.2f, 1.0f, 0.4f));
        setP (p, pid (Filter, ids::filterFreq), 6000.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("36 Nebula Dust");
        addLayer (p, Grain, 70.0f, 2, flat (1.0f));
        setP (p, pid (Grain, ids::grainSize), 180.0f);
        setP (p, pid (Grain, ids::grainOvl), 4.0f);
        setP (p, pid (Grain, ids::grainRand), 3.0f);
        setP (p, pid (Grain, ids::grainSpray), 120.0f);
        setP (p, pid (Grain, ids::grainTime), 300.0f);
        setP (p, pid (Grain, ids::grainFB), 40.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("37 Octave Comets");
        addLayer (p, Grain, 75.0f, 2, flat (1.0f));
        setP (p, pid (Grain, ids::grainSize), 120.0f);
        setP (p, pid (Grain, ids::grainOvl), 3.0f);
        setP (p, pid (Grain, ids::grainPitch), 12.0f);
        setP (p, pid (Grain, ids::grainRand), 2.0f);
        setP (p, pid (Grain, ids::grainSpray), 60.0f);
        setP (p, pid (Grain, ids::grainTime), 150.0f);
        setP (p, pid (Grain, ids::grainFB), 30.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("38 Granular Stutter");
        addLayer (p, Grain, 85.0f, 6, gate (0.5f, 1.0f, 0.0f));
        setP (p, pid (Grain, ids::grainSize), 60.0f);
        setP (p, pid (Grain, ids::grainOvl), 6.0f);
        setP (p, pid (Grain, ids::grainTime), 120.0f);
        setP (p, pid (Grain, ids::grainFB), 55.0f);
        setP (p, ids::smooth, 1.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("39 Space Ambience");
        addLayer (p, Reverb, 45.0f, 1, flat (1.0f));
        setP (p, pid (Reverb, ids::reverbSize), 95.0f);
        setP (p, pid (Reverb, ids::reverbDamp), 55.0f);
        setP (p, pid (Reverb, ids::reverbWidth), 100.0f);
        setP (p, pid (Reverb, ids::reverbPreDelay), 80.0f);
        list.push_back (std::move (p));
    }
    {
        auto p = next ("40 Event Horizon");
        addLayer (p, Volume, 90.0f, 6, gate (0.6f, 1.0f, 0.1f));
        addLayer (p, Filter, 80.0f, 5, sine (16, 0.1f, 1.0f));
        setP (p, pid (Filter, ids::filterFreq), 3000.0f);
        setP (p, pid (Filter, ids::filterRes), 60.0f);
        addLayer (p, Drive, 60.0f, 4, flat (0.7f));
        addLayer (p, Crush, 50.0f, 7, steps ({ 1.0f, 0.2f, 0.6f, 0.3f }));
        setP (p, pid (Crush, ids::crushBits), 8.0f);
        addLayer (p, Pan, 80.0f, 4, sine (16));
        addLayer (p, Grain, 40.0f, 1, flat (0.8f));
        setP (p, pid (Grain, ids::grainSize), 150.0f);
        setP (p, pid (Grain, ids::grainOvl), 4.0f);
        addLayer (p, Delay, 50.0f, 5, flat (0.9f));
        setP (p, pid (Delay, ids::delayTime), 5.0f);
        setP (p, pid (Delay, ids::delayFB), 50.0f);
        setP (p, pid (Delay, ids::delayPP), 1.0f);
        addLayer (p, Reverb, 60.0f, 2, flat (1.0f));
        setP (p, pid (Reverb, ids::reverbSize), 88.0f);
        setP (p, ids::smooth, 3.0f);
        setP (p, ids::output, -5.0f);
        list.push_back (std::move (p));
    }

    return list;
}

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = buildFactoryPresets();
    return presets;
}

} // namespace cb
