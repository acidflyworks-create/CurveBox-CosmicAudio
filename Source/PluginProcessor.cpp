/*
    CurveBox -- PluginProcessor.cpp
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kParamVersion = 1;

    juce::AudioParameterFloatAttributes percentAttrs (const juce::String& label = "%")
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel (label)
            .withStringFromValueFunction ([] (float v, int)
            {
                return juce::String (v, 1) + " %";
            });
    }

    juce::AudioParameterFloatAttributes dbAttrs()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int)
            {
                return juce::String (v, 1) + " dB";
            });
    }

    juce::AudioParameterFloatAttributes msAttrs()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int)
            {
                return juce::String (v, v < 10.0f ? 2 : 1) + " ms";
            });
    }

    juce::AudioParameterFloatAttributes hzAttrs()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int)
            {
                if (v >= 1000.0f)
                    return juce::String (v / 1000.0f, 2) + " kHz";

                return juce::String (v, 0) + " Hz";
            });
    }

    juce::AudioParameterFloatAttributes semitoneAttrs()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("st")
            .withStringFromValueFunction ([] (float v, int)
            {
                return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " st";
            });
    }
}

//==============================================================================
CurveBoxAudioProcessor::CurveBoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CURVEBOX_STATE", createParameterLayout())
{
    cacheParameterPointers();

    curveSmoothed.fill (1.0f);
    prevEnabled.fill (false);

    for (auto& p : guiPhase) p.store (0.0f);
    for (auto& c : guiCurve) c.store (1.0f);

    setLatencySamples (0);
}

CurveBoxAudioProcessor::~CurveBoxAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CurveBoxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //--------------------------------------------------------------------------
    // Global parameters
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::ids::mix, kParamVersion }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f, percentAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::ids::output, kParamVersion }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, dbAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::ids::smooth, kParamVersion }, "Smooth",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.01f), 5.0f, msAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::ids::offset, kParamVersion }, "Offset",
        juce::NormalisableRange<float> (0.0f, 360.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("deg")
            .withStringFromValueFunction ([] (float v, int)
            {
                return juce::String (v, 1) + juce::String::fromUTF8 ("\xc2\xb0");
            })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { cb::ids::midiRetrig, kParamVersion }, "MIDI Retrigger", false));

    //--------------------------------------------------------------------------
    // Per module: Enabled / Depth|Send / Rate
    //--------------------------------------------------------------------------
    for (int m = 0; m < cb::kNumModules; ++m)
    {
        const auto name = cb::moduleName (m);

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { cb::pid (m, cb::ids::enabled), kParamVersion },
            name + " Enabled", false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { cb::pid (m, cb::ids::amount), kParamVersion },
            name + " " + cb::moduleAmountName (m),
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f, percentAttrs()));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { cb::pid (m, cb::ids::rate), kParamVersion },
            name + " Rate", cb::rateNames(), cb::defaultRateIndex (m)));
    }

    //--------------------------------------------------------------------------
    // Filter
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { cb::pid (cb::Filter, cb::ids::filterMode), kParamVersion },
        "Filter Mode", cb::filterModeNames(), 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Filter, cb::ids::filterFreq), kParamVersion },
        "Filter Frequency",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.25f), 2000.0f, hzAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Filter, cb::ids::filterRes), kParamVersion },
        "Filter Resonance",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 20.0f, percentAttrs()));

    //--------------------------------------------------------------------------
    // Drive
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Drive, cb::ids::driveTone), kParamVersion },
        "Drive Tone",
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.01f, 0.3f), 12000.0f, hzAttrs()));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { cb::pid (cb::Drive, cb::ids::driveOS), kParamVersion },
        "Drive 2x Oversampling", true));

    //--------------------------------------------------------------------------
    // Crush
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Crush, cb::ids::crushBits), kParamVersion },
        "Crush Bits",
        juce::NormalisableRange<float> (4.0f, 16.0f, 1.0f), 8.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("bit")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + " bit"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Crush, cb::ids::crushSR), kParamVersion },
        "Crush Downsample",
        juce::NormalisableRange<float> (1.0f, 32.0f, 1.0f), 8.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("x")
            .withStringFromValueFunction ([] (float v, int) { return "1/" + juce::String (v, 0); })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { cb::pid (cb::Crush, cb::ids::crushOS), kParamVersion },
        "Crush 2x Oversampling", true));

    //--------------------------------------------------------------------------
    // Delay
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { cb::pid (cb::Delay, cb::ids::delayTime), kParamVersion },
        "Delay Time", cb::delayTimeNames(), cb::kDefaultDelayTimeIndex));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Delay, cb::ids::delayFB), kParamVersion },
        "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 95.0f, 0.01f), 35.0f, percentAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Delay, cb::ids::delayTone), kParamVersion },
        "Delay Tone",
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.01f, 0.3f), 4000.0f, hzAttrs()));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { cb::pid (cb::Delay, cb::ids::delayPP), kParamVersion },
        "Delay Ping-Pong", false));

    //--------------------------------------------------------------------------
    // Grain
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainSize), kParamVersion },
        "Grain Size", juce::NormalisableRange<float> (20.0f, 500.0f, 0.01f), 120.0f, msAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainOvl), kParamVersion },
        "Grain Overlap",
        juce::NormalisableRange<float> (1.0f, 8.0f, 0.01f), 2.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("x")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " x"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainPitch), kParamVersion },
        "Grain Pitch", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, semitoneAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainRand), kParamVersion },
        "Grain Random Pitch", juce::NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f, semitoneAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainSpray), kParamVersion },
        "Grain Spray", juce::NormalisableRange<float> (0.0f, 500.0f, 0.01f), 0.0f, msAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainTime), kParamVersion },
        "Grain Time", juce::NormalisableRange<float> (0.0f, 1000.0f, 0.01f), 200.0f, msAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Grain, cb::ids::grainFB), kParamVersion },
        "Grain Feedback",
        juce::NormalisableRange<float> (0.0f, 90.0f, 0.01f), 30.0f, percentAttrs()));

    //--------------------------------------------------------------------------
    // Reverb
    //--------------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Reverb, cb::ids::reverbSize), kParamVersion },
        "Reverb Size", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 60.0f, percentAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Reverb, cb::ids::reverbDamp), kParamVersion },
        "Reverb Damping", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 40.0f, percentAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Reverb, cb::ids::reverbWidth), kParamVersion },
        "Reverb Width", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f, percentAttrs()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cb::pid (cb::Reverb, cb::ids::reverbPreDelay), kParamVersion },
        "Reverb Pre-Delay", juce::NormalisableRange<float> (0.0f, 200.0f, 0.01f), 20.0f, msAttrs()));

    return layout;
}

//==============================================================================
void CurveBoxAudioProcessor::cacheParameterPointers()
{
    for (int m = 0; m < cb::kNumModules; ++m)
    {
        raw.enabled[m] = apvts.getRawParameterValue (cb::pid (m, cb::ids::enabled));
        raw.amount[m]  = apvts.getRawParameterValue (cb::pid (m, cb::ids::amount));
        raw.rate[m]    = apvts.getRawParameterValue (cb::pid (m, cb::ids::rate));
    }

    raw.mix            = apvts.getRawParameterValue (cb::ids::mix);
    raw.output         = apvts.getRawParameterValue (cb::ids::output);
    raw.smooth         = apvts.getRawParameterValue (cb::ids::smooth);
    raw.offset         = apvts.getRawParameterValue (cb::ids::offset);
    raw.midiRetrigger  = apvts.getRawParameterValue (cb::ids::midiRetrig);

    raw.filterMode = apvts.getRawParameterValue (cb::pid (cb::Filter, cb::ids::filterMode));
    raw.filterFreq = apvts.getRawParameterValue (cb::pid (cb::Filter, cb::ids::filterFreq));
    raw.filterRes  = apvts.getRawParameterValue (cb::pid (cb::Filter, cb::ids::filterRes));

    raw.driveTone = apvts.getRawParameterValue (cb::pid (cb::Drive, cb::ids::driveTone));
    raw.driveOS   = apvts.getRawParameterValue (cb::pid (cb::Drive, cb::ids::driveOS));

    raw.crushBits = apvts.getRawParameterValue (cb::pid (cb::Crush, cb::ids::crushBits));
    raw.crushSR   = apvts.getRawParameterValue (cb::pid (cb::Crush, cb::ids::crushSR));
    raw.crushOS   = apvts.getRawParameterValue (cb::pid (cb::Crush, cb::ids::crushOS));

    raw.delayTime = apvts.getRawParameterValue (cb::pid (cb::Delay, cb::ids::delayTime));
    raw.delayFB   = apvts.getRawParameterValue (cb::pid (cb::Delay, cb::ids::delayFB));
    raw.delayTone = apvts.getRawParameterValue (cb::pid (cb::Delay, cb::ids::delayTone));
    raw.delayPP   = apvts.getRawParameterValue (cb::pid (cb::Delay, cb::ids::delayPP));

    raw.grainSize  = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainSize));
    raw.grainOvl   = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainOvl));
    raw.grainPitch = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainPitch));
    raw.grainRand  = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainRand));
    raw.grainSpray = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainSpray));
    raw.grainTime  = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainTime));
    raw.grainFB    = apvts.getRawParameterValue (cb::pid (cb::Grain, cb::ids::grainFB));

    raw.reverbSize     = apvts.getRawParameterValue (cb::pid (cb::Reverb, cb::ids::reverbSize));
    raw.reverbDamp     = apvts.getRawParameterValue (cb::pid (cb::Reverb, cb::ids::reverbDamp));
    raw.reverbWidth    = apvts.getRawParameterValue (cb::pid (cb::Reverb, cb::ids::reverbWidth));
    raw.reverbPreDelay = apvts.getRawParameterValue (cb::pid (cb::Reverb, cb::ids::reverbPreDelay));
}

//==============================================================================
void CurveBoxAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    filterL.reset();
    filterR.reset();
    driveProc.prepare (currentSampleRate);
    crushProc.prepare (currentSampleRate);
    delayProc.prepare (currentSampleRate, 4.0);
    grainProc.prepare (currentSampleRate);
    reverbProc.prepare (currentSampleRate);

    internalBeats = 0.0;
    curveSmoothed.fill (1.0f);
    prevEnabled.fill (false);
}

void CurveBoxAudioProcessor::releaseResources()
{
    filterL.reset();
    filterR.reset();
    delayProc.reset();
    grainProc.reset();
    reverbProc.reset();
}

bool CurveBoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}

//==============================================================================
void CurveBoxAudioProcessor::resetModuleBuffers (int module)
{
    switch (module)
    {
        case cb::Filter: filterL.reset(); filterR.reset(); break;
        case cb::Drive:  driveProc.reset(); break;
        case cb::Crush:  crushProc.reset(); break;
        case cb::Delay:  delayProc.reset(); break;
        case cb::Grain:  grainProc.reset(); break;
        case cb::Reverb: reverbProc.reset(); break;
        case cb::Pan:
        case cb::Volume:
        default: break;
    }
}

//==============================================================================
void CurveBoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    //--------------------------------------------------------------------------
    // Optional: restart all curves on a MIDI note-on
    //--------------------------------------------------------------------------
    if (raw.midiRetrigger->load (std::memory_order_relaxed) > 0.5f)
    {
        for (const auto metadata : midiMessages)
        {
            if (metadata.getMessage().isNoteOn())
            {
                internalBeats = 0.0;
                break;
            }
        }
    }

    //--------------------------------------------------------------------------
    // Transport
    //--------------------------------------------------------------------------
    double bpm = 120.0;
    double ppqPosition = 0.0;
    bool isPlaying = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = juce::jlimit (20.0, 300.0, *hostBpm);

            if (const auto ppq = position->getPpqPosition())
                ppqPosition = *ppq;

            isPlaying = position->getIsPlaying();
        }
    }

    const double beatsPerSample = bpm / (60.0 * currentSampleRate);

    if (isPlaying)
        internalBeats = ppqPosition;

    const double blockStartBeats = isPlaying ? ppqPosition : internalBeats;

    if (! isPlaying)
        internalBeats += beatsPerSample * (double) numSamples;

    //--------------------------------------------------------------------------
    // Parameters (read once per block)
    //--------------------------------------------------------------------------
    const float mixAmount   = juce::jlimit (0.0f, 1.0f, raw.mix->load() * 0.01f);
    const float outputGain  = juce::Decibels::decibelsToGain (raw.output->load());
    const float smoothMs    = juce::jmax (0.0f, raw.smooth->load());
    const float offsetPhase = raw.offset->load() / 360.0f;

    std::array<bool, cb::kNumModules>  enabled { };
    std::array<float, cb::kNumModules> amount { };
    std::array<double, cb::kNumModules> cycleBeats { };

    for (int m = 0; m < cb::kNumModules; ++m)
    {
        enabled[m] = raw.enabled[m]->load() > 0.5f;
        amount[m]  = juce::jlimit (0.0f, 1.0f, raw.amount[m]->load() * 0.01f);
        cycleBeats[m] = cb::rateBeats()[(size_t) juce::jlimit (0, (int) cb::rateBeats().size() - 1,
                                                             (int) std::lround (raw.rate[m]->load()))];

        if (! enabled[m] && prevEnabled[m])
            resetModuleBuffers (m);

        prevEnabled[m] = enabled[m];
    }

    float curveCoeff = 1.0f;

    if (smoothMs > 0.05f)
        curveCoeff = 1.0f - std::exp (-1.0f / (0.001f * smoothMs * (float) currentSampleRate));

    // Filter
    const float filterFreq = juce::jlimit (20.0f, 20000.0f, raw.filterFreq->load());
    const float filterRes  = juce::jlimit (0.0f, 1.0f, raw.filterRes->load() * 0.01f);
    const int   filterMode = juce::jlimit (0, 2, (int) std::lround (raw.filterMode->load()));

    // Drive / Crush
    driveProc.setTone (raw.driveTone->load());
    const bool driveOS = raw.driveOS->load() > 0.5f;
    const float crushBits = raw.crushBits->load();
    const float crushSR   = raw.crushSR->load();
    const bool crushOS = raw.crushOS->load() > 0.5f;

    // Delay
    {
        const int timeIndex = juce::jlimit (0, (int) cb::delayTimeBeats().size() - 1,
                                           (int) std::lround (raw.delayTime->load()));
        const double seconds = cb::delayTimeBeats()[(size_t) timeIndex] * 60.0 / bpm;
        delayProc.setTargetDelaySeconds ((float) seconds);
        delayProc.setTone (raw.delayTone->load());
    }

    const float delayFB = juce::jlimit (0.0f, 0.95f, raw.delayFB->load() * 0.01f);
    const bool  delayPP = raw.delayPP->load() > 0.5f;

    // Grain
    const float grainSize  = raw.grainSize->load();
    const float grainOvl   = raw.grainOvl->load();
    const float grainPitch = raw.grainPitch->load();
    const float grainRand  = raw.grainRand->load();
    const float grainSpray = raw.grainSpray->load();
    const float grainTime  = raw.grainTime->load();
    const float grainFB    = raw.grainFB->load() * 0.01f;

    // Reverb
    reverbProc.setParams (raw.reverbSize->load() * 0.01f,
                          raw.reverbDamp->load() * 0.01f,
                          raw.reverbWidth->load() * 0.01f,
                          raw.reverbPreDelay->load());

    //--------------------------------------------------------------------------
    // Audio
    //--------------------------------------------------------------------------
    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        const double beat = blockStartBeats + (double) s * beatsPerSample;

        float cv[cb::kNumModules];

        for (int m = 0; m < cb::kNumModules; ++m)
        {
            double phase = beat / cycleBeats[m] + (double) offsetPhase;
            phase -= std::floor (phase);

            const float target = curves[(size_t) m].sample ((float) phase);
            curveSmoothed[(size_t) m] += curveCoeff * (target - curveSmoothed[(size_t) m]);
            cv[m] = curveSmoothed[(size_t) m];
        }

        float l = left[s];
        float r = right != nullptr ? right[s] : l;

        const float dryL = l;
        const float dryR = r;

        // 1) Filter ------------------------------------------------------------
        if (enabled[cb::Filter])
        {
            const float open = 1.0f - amount[cb::Filter] * (1.0f - cv[cb::Filter]);
            float cutoff;

            if (filterMode == 1) // high pass: "open" means the cutoff drops
                cutoff = 20.0f * std::pow (juce::jmax (20.0f, filterFreq) / 20.0f, 1.0f - open);
            else
                cutoff = juce::jmax (20.0f, filterFreq * std::pow (0.02f, 1.0f - open));

            filterL.setCoefficients (cutoff, filterRes, currentSampleRate);
            filterR.setCoefficients (cutoff, filterRes, currentSampleRate);

            float lp, bp, hp;

            filterL.process (l, lp, bp, hp);
            l = filterMode == 0 ? lp : (filterMode == 1 ? hp : bp);

            filterR.process (r, lp, bp, hp);
            r = filterMode == 0 ? lp : (filterMode == 1 ? hp : bp);
        }

        // 2) Drive -------------------------------------------------------------
        if (enabled[cb::Drive])
        {
            const float amt = amount[cb::Drive] * cv[cb::Drive];
            l = driveProc.process (0, l, amt, driveOS);
            r = driveProc.process (1, r, amt, driveOS);
        }

        // 3) Crush -------------------------------------------------------------
        if (enabled[cb::Crush])
        {
            const float amt = amount[cb::Crush] * cv[cb::Crush];
            l = crushProc.process (0, l, amt, crushBits, crushSR, crushOS);
            r = crushProc.process (1, r, amt, crushBits, crushSR, crushOS);
        }

        // 4) Grain send --------------------------------------------------------
        if (enabled[cb::Grain])
        {
            const float send = amount[cb::Grain] * cv[cb::Grain];
            float wetL = 0.0f, wetR = 0.0f;

            grainProc.process (l * send, r * send,
                               grainSize, grainOvl, grainPitch, grainRand,
                               grainSpray, grainTime, grainFB,
                               wetL, wetR);

            l += wetL;
            r += wetR;
        }

        // 5) Delay send --------------------------------------------------------
        if (enabled[cb::Delay])
        {
            const float send = amount[cb::Delay] * cv[cb::Delay];
            float wetL = 0.0f, wetR = 0.0f;

            delayProc.process (l * send, r * send, delayFB, delayPP, wetL, wetR);

            l += wetL;
            r += wetR;
        }

        // 6) Reverb send -------------------------------------------------------
        if (enabled[cb::Reverb])
        {
            const float send = amount[cb::Reverb] * cv[cb::Reverb];
            float wetL = 0.0f, wetR = 0.0f;

            reverbProc.process (l * send, r * send, wetL, wetR);

            l += wetL;
            r += wetR;
        }

        // 7) Pan ---------------------------------------------------------------
        if (enabled[cb::Pan])
        {
            const float pos = juce::jlimit (0.0f, 1.0f, 0.5f + (cv[cb::Pan] - 0.5f) * amount[cb::Pan]);
            float gainL, gainR;
            cb::equalPowerPan (pos, gainL, gainR);
            l *= gainL;
            r *= gainR;
        }

        // 8) Volume ------------------------------------------------------------
        if (enabled[cb::Volume])
        {
            const float gain = juce::jlimit (0.0f, 1.0f, 1.0f - amount[cb::Volume] * (1.0f - cv[cb::Volume]));
            l *= gain;
            r *= gain;
        }

        // Dry/wet and output ---------------------------------------------------
        left[s] = (dryL + mixAmount * (l - dryL)) * outputGain;

        if (right != nullptr)
            right[s] = (dryR + mixAmount * (r - dryR)) * outputGain;
    }

    //--------------------------------------------------------------------------
    // Publish phase / curve values for the editor
    //--------------------------------------------------------------------------
    const double endBeat = blockStartBeats + (double) numSamples * beatsPerSample;

    for (int m = 0; m < cb::kNumModules; ++m)
    {
        double phase = endBeat / cycleBeats[m] + (double) offsetPhase;
        phase -= std::floor (phase);

        guiPhase[(size_t) m].store ((float) phase, std::memory_order_relaxed);
        guiCurve[(size_t) m].store (curveSmoothed[(size_t) m], std::memory_order_relaxed);
    }
}

//==============================================================================
float CurveBoxAudioProcessor::getParamValue (const juce::String& paramID) const
{
    if (auto* param = apvts.getParameter (paramID))
        return param->convertFrom0to1 (param->getValue());

    return 0.0f;
}

//==============================================================================
int CurveBoxAudioProcessor::getNumPresets() const
{
    return (int) cb::factoryPresets().size();
}

juce::StringArray CurveBoxAudioProcessor::getPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : cb::factoryPresets())
        names.add (preset.name);

    return names;
}

//==============================================================================
int CurveBoxAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, getNumPresets());
}

int CurveBoxAudioProcessor::getCurrentProgram()
{
    return currentPreset.load();
}

void CurveBoxAudioProcessor::setCurrentProgram (int index)
{
    loadPreset (index);
}

const juce::String CurveBoxAudioProcessor::getProgramName (int index)
{
    const auto& presets = cb::factoryPresets();

    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;

    return {};
}

void CurveBoxAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
    // Factory presets are read-only; user presets are saved to files instead.
}

//==============================================================================
void CurveBoxAudioProcessor::loadPreset (int index)
{
    const int numPresets = getNumPresets();

    if (numPresets <= 0)
        return;

    index = juce::jlimit (0, numPresets - 1, index);

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        applyPreset (index);
    }
    else
    {
        pendingPresetIndex = index;
        triggerAsyncUpdate();
    }
}

void CurveBoxAudioProcessor::applyPreset (int index)
{
    const auto& presets = cb::factoryPresets();

    if (presets.empty())
        return;

    index = juce::jlimit (0, (int) presets.size() - 1, index);
    const auto& preset = presets[(size_t) index];

    // 1) every parameter back to its default, so nothing leaks between presets
    for (auto* param : getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());

    // 2) preset values (real, user facing units)
    for (const auto& entry : preset.params)
    {
        if (auto* param = apvts.getParameter (entry.first))
            param->setValueNotifyingHost (param->convertTo0to1 (entry.second));
    }

    // 3) curves
    for (int m = 0; m < cb::kNumModules; ++m)
        curves[(size_t) m].setPoints (preset.curves[(size_t) m]);

    currentPreset.store (index);

    for (int m = 0; m < cb::kNumModules; ++m)
        resetModuleBuffers (m);

    if (onPresetChanged != nullptr)
        onPresetChanged();
}

void CurveBoxAudioProcessor::notifyCurveEdited (int module)
{
    if (onCurveEdited != nullptr)
        onCurveEdited (module);
}

void CurveBoxAudioProcessor::handleAsyncUpdate()
{
    if (pendingPresetIndex >= 0)
    {
        const int index = pendingPresetIndex;
        pendingPresetIndex = -1;
        applyPreset (index);
    }
}

//==============================================================================
void CurveBoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr)
        return;

    auto* curvesXml = xml->createNewChildElement ("CURVES");
    curvesXml->setAttribute ("snap", snapEnabled.load() ? 1 : 0);
    curvesXml->setAttribute ("preset", currentPreset.load());

    for (int m = 0; m < cb::kNumModules; ++m)
        curves[(size_t) m].writeToXml (*curvesXml, "m" + juce::String (m));

    copyXmlToBinary (*xml, destData);
}

void CurveBoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    auto curvesXml = std::unique_ptr<juce::XmlElement> (xml->getChildByName ("CURVES"));

    if (curvesXml != nullptr)
    {
        for (int m = 0; m < cb::kNumModules; ++m)
            curves[(size_t) m].readFromXml (curvesXml->getChildByName ("m" + juce::String (m)));

        snapEnabled.store (curvesXml->getIntAttribute ("snap", 1) != 0);
        currentPreset.store (juce::jlimit (0, juce::jmax (0, getNumPresets() - 1),
                                           curvesXml->getIntAttribute ("preset", 0)));
    }

    // Remove the curve data before handing the XML back to the APVTS.
    if (auto* child = xml->getChildByName ("CURVES"))
        xml->removeChildElement (child, true);

    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    else
        apvts.replaceState (juce::ValueTree::fromXml (*xml)); // best effort on foreign tags
}

//==============================================================================
juce::AudioProcessorEditor* CurveBoxAudioProcessor::createEditor()
{
    return new CurveBoxAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CurveBoxAudioProcessor();
}
