/*
    CurveBox -- Curve.h
    A drawable, tempo-locked curve with a lock-free LUT for the audio thread.

    Design:
      * The message thread owns the point list and rebuilds a 1024-entry LUT
        whenever it changes.
      * Two LUT slots are used; the audio thread reads the slot indicated by an
        atomic index, the writer always fills the *other* slot and then flips the
        index with release semantics. No locks, no allocations on the audio side.
*/

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace cb
{

class CurveShape
{
public:
    static constexpr int kLutSize   = 1024;
    static constexpr int kMaxPoints = 32;

    struct Point
    {
        float x = 0.0f;        // 0..1 phase
        float y = 1.0f;        // 0..1 value
        float tension = 0.5f;  // 0..1 curvature of the segment to the NEXT point (0.5 = linear)
    };

    enum class ShapeType
    {
        Flat,
        SawDown,
        SawUp,
        Square,
        Triangle,
        Sine,
        Steps,
        RandomSteps
    };

    //==========================================================================
    CurveShape()
    {
        setPoints (makeDefaultPoints());
    }

    static std::vector<Point> makeDefaultPoints()
    {
        return { { 0.0f, 1.0f, 0.5f }, { 1.0f, 1.0f, 0.5f } };
    }

    //==========================================================================
    // Point access (message thread)
    //==========================================================================
    int getNumPoints() const noexcept { return (int) points.size(); }

    Point getPoint (int index) const
    {
        if (index < 0 || index >= (int) points.size())
            return {};
        return points[(size_t) index];
    }

    const std::vector<Point>& getPoints() const noexcept { return points; }

    void setPoints (std::vector<Point> newPoints)
    {
        if (newPoints.empty())
            newPoints = makeDefaultPoints();

        for (auto& p : newPoints)
        {
            p.x = juce::jlimit (0.0f, 1.0f, p.x);
            p.y = juce::jlimit (0.0f, 1.0f, p.y);
            p.tension = juce::jlimit (0.0f, 1.0f, p.tension);
        }

        std::sort (newPoints.begin(), newPoints.end(),
                   [] (const Point& a, const Point& b) { return a.x < b.x; });

        if ((int) newPoints.size() > kMaxPoints)
            newPoints.resize ((size_t) kMaxPoints);

        // The curve must span the whole cycle so that it never "falls off" an edge.
        newPoints.front().x = 0.0f;
        newPoints.back().x  = 1.0f;

        points = std::move (newPoints);
        rebuild();
    }

    /** Adds a point, keeping the list sorted. Returns the index of the new point. */
    int addPoint (float x, float y)
    {
        if ((int) points.size() >= kMaxPoints)
            return -1;

        auto pts = points;
        pts.push_back ({ juce::jlimit (0.0f, 1.0f, x), juce::jlimit (0.0f, 1.0f, y), 0.5f });
        setPoints (std::move (pts));

        for (int i = 0; i < getNumPoints(); ++i)
            if (std::abs (points[(size_t) i].x - x) < 1.0e-5f)
                return i;

        return -1;
    }

    /** Removes an inner point. Returns true on success (the two edge points stay). */
    bool removePoint (int index)
    {
        if (index <= 0 || index >= getNumPoints() - 1)
            return false;

        auto pts = points;
        pts.erase (pts.begin() + index);
        setPoints (std::move (pts));
        return true;
    }

    void movePoint (int index, float x, float y)
    {
        if (index < 0 || index >= getNumPoints())
            return;

        auto pts = points;
        auto& p = pts[(size_t) index];
        p.y = juce::jlimit (0.0f, 1.0f, y);

        // Edge points keep their phase; inner points are clamped between neighbours.
        if (index > 0 && index < getNumPoints() - 1)
        {
            const float lower = pts[(size_t) index - 1].x + 0.001f;
            const float upper = pts[(size_t) index + 1].x - 0.001f;
            p.x = juce::jlimit (lower, upper, x);
        }

        setPoints (std::move (pts));
    }

    void setTension (int segmentIndex, float tension)
    {
        if (segmentIndex < 0 || segmentIndex >= getNumPoints() - 1)
            return;

        auto pts = points;
        pts[(size_t) segmentIndex].tension = juce::jlimit (0.0f, 1.0f, tension);
        setPoints (std::move (pts));
    }

    float getTension (int segmentIndex) const
    {
        if (segmentIndex < 0 || segmentIndex >= getNumPoints() - 1)
            return 0.5f;
        return points[(size_t) segmentIndex].tension;
    }

    //==========================================================================
    // Shapes
    //==========================================================================
    void applyShape (ShapeType type)
    {
        std::vector<Point> pts;

        switch (type)
        {
            case ShapeType::Flat:
                pts = { { 0.0f, 1.0f, 0.5f }, { 1.0f, 1.0f, 0.5f } };
                break;

            case ShapeType::SawDown:
                pts = { { 0.0f, 1.0f, 0.5f }, { 1.0f, 0.0f, 0.5f } };
                break;

            case ShapeType::SawUp:
                pts = { { 0.0f, 0.0f, 0.5f }, { 1.0f, 1.0f, 0.5f } };
                break;

            case ShapeType::Square:
                pts = { { 0.0f, 1.0f, 0.5f }, { 0.4999f, 1.0f, 0.5f },
                        { 0.5f, 0.0f, 0.5f }, { 1.0f, 0.0f, 0.5f } };
                break;

            case ShapeType::Triangle:
                pts = { { 0.0f, 0.0f, 0.5f }, { 0.5f, 1.0f, 0.5f }, { 1.0f, 0.0f, 0.5f } };
                break;

            case ShapeType::Sine:
            {
                const int steps = 16;
                for (int i = 0; i <= steps; ++i)
                {
                    const float x = (float) i / (float) steps;
                    const float y = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * x
                                                            - juce::MathConstants<float>::halfPi);
                    pts.push_back ({ x, y, 0.5f });
                }
                break;
            }

            case ShapeType::Steps:
            {
                const int steps = 8;
                for (int i = 0; i < steps; ++i)
                {
                    const float x0 = (float) i / (float) steps;
                    const float x1 = (float) (i + 1) / (float) steps - 0.0001f;
                    const float y  = (i % 2 == 0) ? 1.0f : 0.25f;
                    pts.push_back ({ x0, y, 0.5f });
                    pts.push_back ({ x1, y, 0.5f });
                }
                pts.front().x = 0.0f;
                break;
            }

            case ShapeType::RandomSteps:
            {
                juce::Random rng (0x51ab7e);
                const int steps = 8;
                for (int i = 0; i < steps; ++i)
                {
                    const float x0 = (float) i / (float) steps;
                    const float x1 = (float) (i + 1) / (float) steps - 0.0001f;
                    const float y  = juce::jlimit (0.0f, 1.0f, rng.nextFloat() * 1.2f - 0.1f);
                    pts.push_back ({ x0, y, 0.5f });
                    pts.push_back ({ x1, y, 0.5f });
                }
                pts.front().x = 0.0f;
                break;
            }
        }

        if (pts.size() > (size_t) kMaxPoints)
            pts.resize ((size_t) kMaxPoints);

        setPoints (std::move (pts));
    }

    void setFlat (float y)
    {
        const float v = juce::jlimit (0.0f, 1.0f, y);
        setPoints ({ { 0.0f, v, 0.5f }, { 1.0f, v, 0.5f } });
    }

    //==========================================================================
    // Evaluation
    //==========================================================================
    static float evaluate (float phase, const std::vector<Point>& pts) noexcept
    {
        if (pts.empty())
            return 1.0f;

        phase = juce::jlimit (0.0f, 1.0f, phase);

        if (phase <= pts.front().x)
            return pts.front().y;

        if (phase >= pts.back().x)
            return pts.back().y;

        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const auto& a = pts[i];
            const auto& b = pts[i + 1];

            if (phase >= a.x && phase <= b.x)
            {
                const float dx = b.x - a.x;

                if (dx <= 1.0e-6f)
                    return b.y;

                float u = (phase - a.x) / dx;

                // tension 0.5 -> exponent 1.0 (linear); 0 -> x^4; 1 -> x^0.25
                const float exponent = std::pow (2.0f, (0.5f - juce::jlimit (0.0f, 1.0f, a.tension)) * 4.0f);
                u = std::pow (juce::jlimit (0.0f, 1.0f, u), exponent);

                return juce::jlimit (0.0f, 1.0f, a.y + (b.y - a.y) * u);
            }
        }

        return pts.back().y;
    }

    /** Audio-thread safe look-up: linear interpolation inside the published LUT. */
    float sample (float phase01) const noexcept
    {
        const auto& table = lut[(size_t) activeSlot.load (std::memory_order_acquire)];

        const float p = phase01 - std::floor (phase01);
        const float x = p * (float) (kLutSize - 1);
        const int   i0 = juce::jlimit (0, kLutSize - 1, (int) x);
        const int   i1 = juce::jmin (kLutSize - 1, i0 + 1);
        const float frac = x - (float) i0;

        return table[(size_t) i0] + frac * (table[(size_t) i1] - table[(size_t) i0]);
    }

    /** Message thread: rebuild the LUT from the current point list. */
    void rebuild()
    {
        const int writeSlot = 1 - activeSlot.load (std::memory_order_relaxed);
        auto& table = lut[(size_t) writeSlot];

        for (int i = 0; i < kLutSize; ++i)
            table[(size_t) i] = evaluate ((float) i / (float) (kLutSize - 1), points);

        activeSlot.store (writeSlot, std::memory_order_release);
        ++editCounter;
    }

    int getEditCounter() const noexcept { return editCounter; }

    //==========================================================================
    // State
    //==========================================================================
    void writeToXml (juce::XmlElement& parent, const juce::String& tag) const
    {
        auto* curve = parent.createNewChildElement (tag);

        for (const auto& p : points)
        {
            auto* pt = curve->createNewChildElement ("pt");
            pt->setAttribute ("x", (double) p.x);
            pt->setAttribute ("y", (double) p.y);
            pt->setAttribute ("t", (double) p.tension);
        }
    }

    void readFromXml (const juce::XmlElement* curve)
    {
        if (curve == nullptr)
            return;

        std::vector<Point> pts;

        for (auto* pt : curve->getChildIterator())
        {
            if (pt->hasTagName ("pt"))
                pts.push_back ({ (float) pt->getDoubleAttribute ("x", 0.0),
                                 (float) pt->getDoubleAttribute ("y", 1.0),
                                 (float) pt->getDoubleAttribute ("t", 0.5) });
        }

        if (pts.size() < 2)
            pts = makeDefaultPoints();

        setPoints (std::move (pts));
    }

private:
    std::vector<Point> points;
    std::array<std::array<float, (size_t) kLutSize>, 2> lut { };
    std::atomic<int> activeSlot { 0 };
    int editCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveShape)
};

} // namespace cb
