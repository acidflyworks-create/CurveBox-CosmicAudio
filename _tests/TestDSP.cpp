// CurveBox DSP smoke test (not part of the plugin build).
// Compile: see _tests/build_and_run.sh

#include "Effects.h"
#include "Curve.h"
#include "Presets.h"

#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const char* what)
    {
        if (! condition)
        {
            std::printf ("  FAIL: %s\n", what);
            ++failures;
        }
    }

    struct Stats { float peak = 0.0f; double rms = 0.0; bool finite = true; };

    void accumulate (Stats& s, float v)
    {
        if (! std::isfinite (v))
        {
            s.finite = false;
            return;
        }

        s.peak = juce::jmax (s.peak, std::abs (v));
        s.rms += (double) v * (double) v;
    }

    float sine (double t, double freq = 220.0)
    {
        return (float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * freq * t));
    }
}

int main()
{
    constexpr double sr = 48000.0;

    //--------------------------------------------------------------------------
    // Curve
    //--------------------------------------------------------------------------
    {
        cb::CurveShape curve;
        check (curve.getNumPoints() == 2, "default curve has two points");
        check (std::abs (curve.sample (0.5f) - 1.0f) < 1.0e-4f, "flat default curve is 1.0");

        curve.applyShape (cb::CurveShape::ShapeType::SawDown);
        check (curve.sample (0.02f) > 0.9f, "saw down starts high");
        check (curve.sample (0.98f) < 0.1f, "saw down ends low");

        curve.applyShape (cb::CurveShape::ShapeType::Sine);
        float minV = 1.0f, maxV = 0.0f;

        for (int i = 0; i < 512; ++i)
        {
            const float v = curve.sample ((float) i / 511.0f);
            minV = juce::jmin (minV, v);
            maxV = juce::jmax (maxV, v);
        }

        check (minV < 0.05f && maxV > 0.95f, "sine spans 0..1");

        curve.applyShape (cb::CurveShape::ShapeType::Flat);
        const int index = curve.addPoint (0.5f, 0.2f);
        check (index > 0, "addPoint returns an inner index");
        check (std::abs (curve.sample (0.5f) - 0.2f) < 0.02f, "added point is honoured");
        check (curve.removePoint (index), "inner point removed");
        check (! curve.removePoint (0), "edge point cannot be removed");

        curve.setPoints ({ { 0.0f, 0.0f, 0.5f }, { 1.0f, 1.0f, 0.5f } });
        check (curve.sample (0.25f) > 0.2f && curve.sample (0.25f) < 0.3f, "linear ramp");

        curve.setTension (0, 0.0f); // exponent 4 -> stays low
        check (curve.sample (0.25f) < 0.02f, "tension bends the segment");
    }

    //--------------------------------------------------------------------------
    // SVF + drive + crush
    //--------------------------------------------------------------------------
    {
        cb::Svf filter;
        filter.setCoefficients (1000.0f, 0.5f, sr);

        Stats s;

        for (int i = 0; i < 4800; ++i)
        {
            float lp, bp, hp;
            filter.process (sine ((double) i / sr), lp, bp, hp);
            accumulate (s, lp);
        }

        check (s.finite, "svf output finite");
        check (s.peak < 1.5f, "svf low pass stays bounded");

        cb::DriveProc drive;
        drive.prepare (sr);
        drive.setTone (6000.0f);
        Stats ds;

        for (int i = 0; i < 4800; ++i)
            accumulate (ds, drive.process (0, sine ((double) i / sr), 0.9f, true));

        check (ds.finite, "drive output finite");
        check (ds.peak <= 1.0001f, "drive output bounded by tanh");

        cb::CrushProc crush;
        crush.prepare (sr);
        Stats cs;

        for (int i = 0; i < 4800; ++i)
            accumulate (cs, crush.process (0, sine ((double) i / sr), 1.0f, 4.0f, 16.0f, true));

        check (cs.finite, "crush output finite");
    }

    //--------------------------------------------------------------------------
    // Delay: tail must continue after the send drops to zero
    //--------------------------------------------------------------------------
    {
        cb::StereoDelay delay;
        delay.prepare (sr, 4.0);
        delay.setTargetDelaySeconds (0.1f);
        delay.setTone (5000.0f);

        Stats first, tail;

        for (int i = 0; i < 4800; ++i) // 100 ms of input
        {
            float wetL, wetR;
            delay.process (sine ((double) i / sr), sine ((double) i / sr), 0.7f, false, wetL, wetR);
            accumulate (first, wetL);
        }

        for (int i = 0; i < 24000; ++i) // 500 ms silence
        {
            float wetL, wetR;
            delay.process (0.0f, 0.0f, 0.7f, false, wetL, wetR);
            accumulate (tail, wetL);
        }

        check (first.finite && tail.finite, "delay output finite");
        check (tail.rms > 0.0, "delay tail rings after the send is zero");
    }

    //--------------------------------------------------------------------------
    // Grain
    //--------------------------------------------------------------------------
    {
        cb::GrainDelay grain;
        grain.prepare (sr);
        Stats s;
        int activeCount = 0;

        for (int i = 0; i < 48000; ++i)
        {
            float wetL, wetR;
            grain.process (sine ((double) i / sr), sine ((double) i / sr),
                           120.0f, 4.0f, 12.0f, 3.0f, 100.0f, 200.0f, 0.5f, wetL, wetR);
            accumulate (s, wetL);

            if (std::abs (wetL) > 1.0e-5f)
                ++activeCount;
        }

        check (s.finite, "grain output finite");
        check (s.peak <= 4.0f, "grain output bounded");
        check (activeCount > 1000, "grain actually produces signal");
    }

    //--------------------------------------------------------------------------
    // Reverb
    //--------------------------------------------------------------------------
    {
        cb::ReverbProc reverb;
        reverb.prepare (sr);
        reverb.setParams (0.9f, 0.3f, 1.0f, 20.0f);
        Stats s;

        for (int i = 0; i < 4800; ++i)
        {
            float wetL, wetR;
            reverb.process (sine ((double) i / sr), sine ((double) i / sr), wetL, wetR);
            accumulate (s, wetL);
        }

        Stats tail;

        for (int i = 0; i < 48000; ++i)
        {
            float wetL, wetR;
            reverb.process (0.0f, 0.0f, wetL, wetR);
            accumulate (tail, wetL);
        }

        check (s.finite && tail.finite, "reverb output finite");
        check (tail.rms > 0.0, "reverb tail rings after the send is zero");
    }

    //--------------------------------------------------------------------------
    // Presets
    //--------------------------------------------------------------------------
    {
        const auto& presets = cb::factoryPresets();
        std::printf ("presets: %d\n", (int) presets.size());
        check (presets.size() == 40, "40 factory presets");

        for (const auto& preset : presets)
        {
            check (preset.name.isNotEmpty(), "preset has a name");
            check (! preset.params.empty() || preset.name.startsWith ("01"), "preset sets parameters");

            for (const auto& c : preset.curves)
            {
                check (c.size() >= 2, "preset curve has at least two points");
                check (c.front().x == 0.0f && c.back().x == 1.0f, "preset curve spans the cycle");

                for (const auto& pt : c)
                    check (pt.y >= 0.0f && pt.y <= 1.0f, "preset curve point within 0..1");
            }
        }

        for (size_t i = 1; i < presets.size(); ++i)
            check (presets[i].name != presets[i - 1].name, "preset names unique");

        std::printf ("first: %s / last: %s\n",
                     presets.front().name.toRawUTF8(), presets.back().name.toRawUTF8());
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
