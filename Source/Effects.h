/*
    CurveBox -- Effects.h
    All DSP building blocks. Header-only, allocation happens only in prepare().

    Everything is deliberately sampled-based (process one frame at a time) so the
    processor can modulate every stage with the curve LUT at audio rate.
*/

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

namespace cb
{

//==============================================================================
/** Topology-preserving state variable filter (Zavalishin / Simper style). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float lastCutoff = -1.0f, lastRes = -1.0f;

    void reset() noexcept
    {
        ic1 = ic2 = 0.0f;
    }

    void setCoefficients (float cutoffHz, float res01, double sampleRate) noexcept
    {
        const float nyquist = (float) (sampleRate * 0.5);
        cutoffHz = juce::jlimit (12.0f, nyquist * 0.98f, cutoffHz);
        res01 = juce::jlimit (0.0f, 1.0f, res01);

        if (lastCutoff > 0.0f
            && std::abs (cutoffHz - lastCutoff) < lastCutoff * 0.002f
            && std::abs (res01 - lastRes) < 1.0e-5f)
            return;

        lastCutoff = cutoffHz;
        lastRes = res01;

        g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);

        const float q = 0.5f + res01 * 15.0f; // Q: 0.5 .. 15.5
        k = 1.0f / q;

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline void process (float v0, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;

        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }

    inline float processLowPass (float v0) noexcept
    {
        float lp, bp, hp;
        process (v0, lp, bp, hp);
        return lp;
    }
};

//==============================================================================
/** Minimal 2x oversampler: zero-stuff, two cascaded SVF low-passes up and down. */
class Oversampler2x
{
public:
    void prepare (double sampleRate)
    {
        const double osRate = sampleRate * 2.0;
        const float  cutoff = (float) (sampleRate * 0.42);

        for (auto& c : stages)
        {
            c.up1.setCoefficients (cutoff, 0.0f, osRate);
            c.up2.setCoefficients (cutoff, 0.0f, osRate);
            c.dn1.setCoefficients (cutoff, 0.0f, osRate);
            c.dn2.setCoefficients (cutoff, 0.0f, osRate);
        }

        ready = true;
    }

    void reset()
    {
        for (auto& c : stages)
        {
            c.up1.reset();
            c.up2.reset();
            c.dn1.reset();
            c.dn2.reset();
        }
    }

    bool isReady() const noexcept { return ready; }

    inline void upsample (int ch, float x, float& s0, float& s1) noexcept
    {
        auto& c = stages[(size_t) juce::jlimit (0, 1, ch)];
        s0 = c.up2.processLowPass (c.up1.processLowPass (x * 2.0f));
        s1 = c.up2.processLowPass (c.up1.processLowPass (0.0f));
    }

    inline float downsample (int ch, float y0, float y1) noexcept
    {
        auto& c = stages[(size_t) juce::jlimit (0, 1, ch)];
        (void) c.dn2.processLowPass (c.dn1.processLowPass (y0));
        return c.dn2.processLowPass (c.dn1.processLowPass (y1));
    }

private:
    struct Stage
    {
        Svf up1, up2, dn1, dn2;
    };

    std::array<Stage, 2> stages;
    bool ready = false;
};

//==============================================================================
/** tanh saturation with automatic loudness compensation (optionally 2x). */
class DriveProc
{
public:
    void prepare (double sampleRate)
    {
        sampleRate = sampleRate;
        oversampler.prepare (sampleRate);
        setTone (12000.0f);
    }

    void reset()
    {
        oversampler.reset();
        toneState[0] = toneState[1] = 0.0f;
    }

    void setTone (float hz)
    {
        const float nyquist = (float) (sampleRate * 0.5);
        hz = juce::jlimit (200.0f, juce::jmin (20000.0f, nyquist * 0.95f), hz);
        toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    }

    /** @param amount 0..1 (curve * depth). */
    inline float process (int ch, float x, float amount, bool useOversampling) noexcept
    {
        const float gain = std::pow (10.0f, juce::jlimit (0.0f, 1.0f, amount) * 24.0f / 20.0f);
        const float comp = std::pow (gain, -0.8f);

        float y;

        if (useOversampling)
        {
            float s0, s1;
            oversampler.upsample (ch, x, s0, s1);
            s0 = std::tanh (s0 * gain);
            s1 = std::tanh (s1 * gain);
            y = oversampler.downsample (ch, s0, s1) * comp;
        }
        else
        {
            y = std::tanh (x * gain) * comp;
        }

        auto& state = toneState[(size_t) juce::jlimit (0, 1, ch)];
        state += toneCoeff * (y - state);
        return state;
    }

private:
    double sampleRate = 44100.0;
    float toneCoeff = 1.0f;
    float toneState[2] { 0.0f, 0.0f };
    Oversampler2x oversampler;
};

//==============================================================================
/** Bit crusher + sample rate reduction. */
class CrushProc
{
public:
    void prepare (double sampleRateIn)
    {
        oversampler.prepare (sampleRateIn);
        reset();
    }

    void reset()
    {
        oversampler.reset();
        holdCounter[0] = holdCounter[1] = 0.0f;
        held[0] = held[1] = 0.0f;
    }

    /** @param amount 0..1, bitDepth destination bit depth (4..16),
        downsample destination factor (1..32). */
    inline float process (int ch, float x, float amount, float bitDepth,
                          float downsampleFactor, bool useOversampling) noexcept
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        const int idx = juce::jlimit (0, 1, ch);

        // sample & hold
        const float factor = 1.0f + amount * (juce::jmax (1.0f, downsampleFactor) - 1.0f);
        holdCounter[idx] -= 1.0f;

        if (holdCounter[idx] <= 0.0f)
        {
            holdCounter[idx] += factor;
            held[idx] = x;
        }

        const float bits = 16.0f - amount * (16.0f - juce::jlimit (1.0f, 16.0f, bitDepth));
        const float levels = std::pow (2.0f, bits) * 0.5f;

        if (useOversampling)
        {
            float s0, s1;
            oversampler.upsample (idx, held[idx], s0, s1);
            s0 = std::round (s0 * levels) / levels;
            s1 = std::round (s1 * levels) / levels;
            return oversampler.downsample (idx, s0, s1);
        }

        return std::round (held[idx] * levels) / levels;
    }

private:
    float holdCounter[2] { 0.0f, 0.0f };
    float held[2] { 0.0f, 0.0f };
    Oversampler2x oversampler;
};

//==============================================================================
/** Equal power pan helper. */
inline void equalPowerPan (float pan01, float& gainL, float& gainR) noexcept
{
    const float angle = juce::jlimit (0.0f, 1.0f, pan01) * juce::MathConstants<float>::halfPi;
    gainL = std::cos (angle);
    gainR = std::sin (angle);
}

//==============================================================================
/** Tempo synced stereo delay with tape style time smoothing and a tone LPF in
    the feedback path. Used as a send effect: dry signal is NOT passed through. */
class StereoDelay
{
public:
    void prepare (double sampleRate, double maxSeconds = 4.0)
    {
        sr = sampleRate;
        const int length = (int) std::ceil (maxSeconds * sampleRate) + 8;

        for (auto& b : buffer)
        {
            b.assign ((size_t) length, 0.0f);
        }

        writePos = 0;
        smoothedDelay = (float) (sampleRate * 0.25);
        targetDelay = smoothedDelay;
        smoothCoeff = 1.0f - std::exp (-1.0f / (0.035f * (float) sampleRate));
        lp[0] = lp[1] = 0.0f;
        setTone (4000.0f);
    }

    void reset()
    {
        for (auto& b : buffer)
            std::fill (b.begin(), b.end(), 0.0f);

        writePos = 0;
        lp[0] = lp[1] = 0.0f;
        smoothedDelay = targetDelay;
    }

    void setTone (float hz)
    {
        const float nyquist = (float) (sr * 0.5);
        hz = juce::jlimit (200.0f, juce::jmin (20000.0f, nyquist * 0.95f), hz);
        toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr);
    }

    void setTargetDelaySeconds (float seconds) noexcept
    {
        const float maxDelay = (float) (buffer[0].size() - 4);
        targetDelay = juce::jlimit (1.0f, maxDelay, seconds * (float) sr);
    }

    inline void process (float inL, float inR, float feedback, bool pingPong,
                         float& outL, float& outR) noexcept
    {
        smoothedDelay += smoothCoeff * (targetDelay - smoothedDelay);

        const float dL = readInterpolated (buffer[0], smoothedDelay);
        const float dR = readInterpolated (buffer[1], smoothedDelay);

        // tone filter inside the feedback loop
        lp[0] += toneCoeff * (dL - lp[0]);
        lp[1] += toneCoeff * (dR - lp[1]);

        const float fbL = pingPong ? lp[1] : lp[0];
        const float fbR = pingPong ? lp[0] : lp[1];

        const float fb = juce::jlimit (0.0f, 0.95f, feedback);

        buffer[0][(size_t) writePos] = std::tanh (inL + fbL * fb);
        buffer[1][(size_t) writePos] = std::tanh (inR + fbR * fb);

        writePos = (writePos + 1) % (int) buffer[0].size();

        outL = dL;
        outR = dR;
    }

private:
    inline float readInterpolated (const std::vector<float>& buf, float delaySamples) const noexcept
    {
        const int len = (int) buf.size();
        float pos = (float) writePos - delaySamples;

        while (pos < 0.0f)
            pos += (float) len;

        while (pos >= (float) len)
            pos -= (float) len;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % len;
        const float frac = pos - (float) i0;

        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    double sr = 44100.0;
    std::array<std::vector<float>, 2> buffer;
    int writePos = 0;
    float smoothedDelay = 0.0f, targetDelay = 0.0f, smoothCoeff = 0.01f;
    float toneCoeff = 0.5f;
    float lp[2] { 0.0f, 0.0f };
};

//==============================================================================
/** Granular delay / grain cloud, up to 24 simultaneous Hann windowed grains. */
class GrainDelay
{
public:
    static constexpr int kMaxGrains = 24;
    static constexpr int kWindowSize = 2048;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufferLength = (int) std::ceil (4.0 * sampleRate) + 8;

        for (auto& b : buffer)
            b.assign ((size_t) bufferLength, 0.0f);

        for (int i = 0; i < kWindowSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                         * (float) i / (float) (kWindowSize - 1));

        writePos = 0;
        samplesToNextGrain = 1.0f;
        grains.fill (Grain { });
        rng.setSeed (0x7a3b17);
    }

    void reset()
    {
        for (auto& b : buffer)
            std::fill (b.begin(), b.end(), 0.0f);

        for (auto& g : grains)
            g = Grain { };

        writePos = 0;
        samplesToNextGrain = 1.0f;
    }

    /** All parameters are per-sample so they can be curve modulated. */
    inline void process (float inL, float inR,
                         float sizeMs, float overlap, float pitchSemis,
                         float randPitchSemis, float sprayMs, float timeMs,
                         float feedback, float& wetL, float& wetR) noexcept
    {
        const float grainLengthF = juce::jlimit (32.0f, (float) bufferLength - 8.0f,
                                                 sizeMs * 0.001f * (float) sr);
        const int grainLength = juce::jmax (32, (int) grainLengthF);
        const float ovl = juce::jlimit (1.0f, 8.0f, overlap);

        samplesToNextGrain -= 1.0f;

        if (samplesToNextGrain <= 0.0f)
        {
            samplesToNextGrain += juce::jmax (1.0f, (float) grainLength / ovl);
            spawnGrain (grainLength, pitchSemis, randPitchSemis, sprayMs, timeMs);
        }

        float accL = 0.0f, accR = 0.0f;

        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            const float w = window[(size_t) juce::jlimit (0, kWindowSize - 1,
                                                          (int) ((float) g.age / (float) g.length
                                                                 * (float) (kWindowSize - 1)))];

            accL += readInterpolated (buffer[0], g.pos) * w * g.panL;
            accR += readInterpolated (buffer[1], g.pos) * w * g.panR;

            g.pos += g.inc;

            while (g.pos >= (double) bufferLength) g.pos -= (double) bufferLength;
            while (g.pos < 0.0)                    g.pos += (double) bufferLength;

            if (++g.age >= g.length)
                g.active = false;
        }

        wetL = accL;
        wetR = accR;

        const float fb = juce::jlimit (0.0f, 0.9f, feedback);

        buffer[0][(size_t) writePos] = std::tanh (inL + fb * accL);
        buffer[1][(size_t) writePos] = std::tanh (inR + fb * accR);

        writePos = (writePos + 1) % bufferLength;
    }

private:
    struct Grain
    {
        bool  active = false;
        double pos = 0.0;
        double inc = 1.0;
        int   length = 1;
        int   age = 0;
        float panL = 0.707f;
        float panR = 0.707f;
    };

    inline float readInterpolated (const std::vector<float>& buf, double pos) const noexcept
    {
        const int i0 = juce::jlimit (0, bufferLength - 1, (int) pos);
        const int i1 = (i0 + 1) % bufferLength;
        const double frac = pos - (double) i0;

        return (float) (buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]));
    }

    void spawnGrain (int grainLength, float pitchSemis, float randPitchSemis,
                     float sprayMs, float timeMs) noexcept
    {
        Grain* target = nullptr;

        for (auto& g : grains)
        {
            if (! g.active)
            {
                target = &g;
                break;
            }
        }

        if (target == nullptr)
        {
            // steal the oldest grain
            int oldest = -1;
            for (int i = 0; i < kMaxGrains; ++i)
            {
                if (oldest < 0 || grains[(size_t) i].age > grains[(size_t) oldest].age)
                    oldest = i;
            }

            target = &grains[(size_t) juce::jmax (0, oldest)];
        }

        const float randPitch = randPitchSemis > 0.0f ? (rng.nextFloat() * 2.0f - 1.0f) * randPitchSemis
                                                      : 0.0f;
        const float semis = juce::jlimit (-36.0f, 36.0f, pitchSemis + randPitch);
        const double ratio = std::pow (2.0, (double) semis / 12.0);

        const float timeSamples = juce::jlimit (0.0f, 1.0f, timeMs * 0.001f) * (float) sr;
        const float spraySamples = rng.nextFloat() * juce::jlimit (0.0f, 500.0f, sprayMs) * 0.001f
                                   * (float) sr;

        double start = (double) writePos - (double) (timeSamples + spraySamples) - (double) grainLength;

        while (start < 0.0)                    start += (double) bufferLength;
        while (start >= (double) bufferLength) start -= (double) bufferLength;

        const float panPos = rng.nextFloat();
        equalPowerPan (panPos, target->panL, target->panR);

        target->active = true;
        target->pos = start;
        target->inc = juce::jlimit (0.06, 16.0, ratio);
        target->length = grainLength;
        target->age = 0;
    }

    double sr = 44100.0;
    int bufferLength = 0;
    int writePos = 0;
    float samplesToNextGrain = 1.0f;
    std::array<std::vector<float>, 2> buffer;
    std::array<Grain, kMaxGrains> grains;
    std::array<float, kWindowSize> window { };
    juce::Random rng;
};

//==============================================================================
/** Freeverb style reverb: 8 combs + 4 allpasses per channel, stereo spread,
    plus a pre-delay line. Used as a send effect. */
class ReverbProc
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sampleRate / 44100.0;

        static const int combTuning[8]  = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apTuning[4]    = { 556, 441, 341, 225 };
        static const int stereoSpread   = 23;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kNumCombs; ++i)
            {
                const int len = (int) std::round ((combTuning[i] + (ch == 1 ? stereoSpread : 0)) * scale);
                combs[(size_t) ch][(size_t) i].init (juce::jmax (8, len));
            }

            for (int i = 0; i < kNumAllpasses; ++i)
            {
                const int len = (int) std::round ((apTuning[i] + (ch == 1 ? stereoSpread : 0)) * scale);
                allpasses[(size_t) ch][(size_t) i].init (juce::jmax (4, len));
            }
        }

        preDelayLength = (int) std::ceil (0.25 * sampleRate) + 8;

        for (auto& p : preDelayBuffer)
        {
            p.assign ((size_t) preDelayLength, 0.0f);
        }

        preDelayWrite = 0;
        preDelaySamples = 0.0f;
        reset();
    }

    void reset()
    {
        for (auto& chCombs : combs)
            for (auto& c : chCombs)
                c.clear();

        for (auto& chAps : allpasses)
            for (auto& a : chAps)
                a.clear();

        for (auto& p : preDelayBuffer)
            std::fill (p.begin(), p.end(), 0.0f);

        preDelayWrite = 0;
    }

    void setParams (float size01, float damping01, float width01, float preDelayMs)
    {
        combFeedback = 0.70f + 0.28f * juce::jlimit (0.0f, 1.0f, size01);
        damp         = juce::jlimit (0.0f, 1.0f, damping01) * 0.4f;
        width        = juce::jlimit (0.0f, 1.0f, width01);
        preDelaySamples = juce::jlimit (0.0f, (float) preDelayLength - 4.0f,
                                        preDelayMs * 0.001f * (float) sr);
    }

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        // pre-delay
        preDelayBuffer[0][(size_t) preDelayWrite] = inL;
        preDelayBuffer[1][(size_t) preDelayWrite] = inR;

        const int readOffset = juce::jlimit (0, preDelayLength - 1, (int) preDelaySamples);
        int readPos = preDelayWrite - readOffset;

        while (readPos < 0)
            readPos += preDelayLength;

        const float pdL = preDelayBuffer[0][(size_t) readPos];
        const float pdR = preDelayBuffer[1][(size_t) readPos];

        preDelayWrite = (preDelayWrite + 1) % preDelayLength;

        float wetL = 0.0f, wetR = 0.0f;

        // combs in parallel
        for (int i = 0; i < kNumCombs; ++i)
            wetL += combs[0][(size_t) i].process (pdL, combFeedback, damp);

        for (int i = 0; i < kNumCombs; ++i)
            wetR += combs[1][(size_t) i].process (pdR, combFeedback, damp);

        // allpasses in series
        for (int i = 0; i < kNumAllpasses; ++i)
            wetL = allpasses[0][(size_t) i].process (wetL);

        for (int i = 0; i < kNumAllpasses; ++i)
            wetR = allpasses[1][(size_t) i].process (wetR);

        const float wet1 = width * 0.5f + 0.5f;
        const float wet2 = (1.0f - width) * 0.5f;

        outL = wetL * wet1 + wetR * wet2;
        outR = wetR * wet1 + wetL * wet2;
    }

private:
    static constexpr int kNumCombs = 8;
    static constexpr int kNumAllpasses = 4;

    struct Comb
    {
        void init (int length)
        {
            buf.assign ((size_t) length, 0.0f);
            idx = 0;
            store = 0.0f;
        }

        void clear()
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            idx = 0;
            store = 0.0f;
        }

        inline float process (float in, float feedback, float damping) noexcept
        {
            const float out = buf[(size_t) idx];
            store = out * (1.0f - damping) + store * damping;
            buf[(size_t) idx] = in + store * feedback;
            idx = (idx + 1) % (int) buf.size();
            return out;
        }

        std::vector<float> buf;
        int idx = 0;
        float store = 0.0f;
    };

    struct Allpass
    {
        void init (int length)
        {
            buf.assign ((size_t) length, 0.0f);
            idx = 0;
        }

        void clear()
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            idx = 0;
        }

        inline float process (float in) noexcept
        {
            const float bufOut = buf[(size_t) idx];
            const float out = -in + bufOut;
            buf[(size_t) idx] = in + bufOut * 0.5f;
            idx = (idx + 1) % (int) buf.size();
            return out;
        }

        std::vector<float> buf;
        int idx = 0;
    };

    double sr = 44100.0;
    std::array<std::array<Comb, kNumCombs>, 2> combs;
    std::array<std::array<Allpass, kNumAllpasses>, 2> allpasses;
    std::array<std::vector<float>, 2> preDelayBuffer;
    int preDelayLength = 0;
    int preDelayWrite = 0;
    float preDelaySamples = 0.0f;
    float combFeedback = 0.84f;
    float damp = 0.2f;
    float width = 1.0f;
};

} // namespace cb
