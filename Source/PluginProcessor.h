/*
    CurveBox -- PluginProcessor.h
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Params.h"
#include "Curve.h"
#include "Effects.h"
#include "Presets.h"

class CurveBoxAudioProcessor : public juce::AudioProcessor,
                               private juce::AsyncUpdater
{
public:
    //==========================================================================
    CurveBoxAudioProcessor();
    ~CurveBoxAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    //--------------------------------------------------------------------------
    // Editor <-> processor interface (message thread only, except the atomics)
    //--------------------------------------------------------------------------
    cb::CurveShape& getCurve (int module) noexcept
    {
        return curves[(size_t) juce::jlimit (0, cb::kNumModules - 1, module)];
    }

    /** Current curve phase of a module (0..1), written by the audio thread. */
    float getModulePhase (int module) const noexcept
    {
        return guiPhase[(size_t) juce::jlimit (0, cb::kNumModules - 1, module)].load (std::memory_order_relaxed);
    }

    /** Last smoothed curve value of a module (0..1), written by the audio thread. */
    float getModuleCurveValue (int module) const noexcept
    {
        return guiCurve[(size_t) juce::jlimit (0, cb::kNumModules - 1, module)].load (std::memory_order_relaxed);
    }

    /** Reads a parameter's current real value by id; 0 if the id is unknown. */
    float getParamValue (const juce::String& paramID) const;

    int getNumPresets() const;
    juce::StringArray getPresetNames() const;
    int getCurrentPresetIndex() const noexcept { return currentPreset; }
    void loadPreset (int index);

    bool isSnapEnabled() const noexcept { return snapEnabled.load(); }
    void setSnapEnabled (bool shouldSnap) noexcept { snapEnabled.store (shouldSnap); }

    /** Called on the message thread whenever the preset changes. */
    std::function<void()> onPresetChanged;

    /** Called on the message thread whenever a curve is edited by the editor. */
    std::function<void(int)> onCurveEdited;

    /** Lets the editor notify the processor that a curve has been changed. */
    void notifyCurveEdited (int module);

private:
    //==========================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();
    void applyPreset (int index);
    void resetModuleBuffers (int module);
    void handleAsyncUpdate() override;

    //==========================================================================
    std::array<cb::CurveShape, cb::kNumModules> curves;

    cb::Svf         filterL, filterR;
    cb::DriveProc   driveProc;
    cb::CrushProc   crushProc;
    cb::StereoDelay delayProc;
    cb::GrainDelay  grainProc;
    cb::ReverbProc  reverbProc;

    double currentSampleRate = 44100.0;
    double internalBeats = 0.0;
    std::array<float, cb::kNumModules> curveSmoothed { };
    std::array<bool, cb::kNumModules> prevEnabled { };

    std::atomic<float> guiPhase[cb::kNumModules];
    std::atomic<float> guiCurve[cb::kNumModules];

    std::atomic<bool> snapEnabled { true };
    std::atomic<int> currentPreset { 0 };

    struct RawParams
    {
        std::atomic<float>* enabled[cb::kNumModules] { };
        std::atomic<float>* amount[cb::kNumModules] { };
        std::atomic<float>* rate[cb::kNumModules] { };

        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* smooth = nullptr;
        std::atomic<float>* offset = nullptr;
        std::atomic<float>* midiRetrigger = nullptr;

        std::atomic<float>* filterMode = nullptr;
        std::atomic<float>* filterFreq = nullptr;
        std::atomic<float>* filterRes = nullptr;

        std::atomic<float>* driveTone = nullptr;
        std::atomic<float>* driveOS = nullptr;

        std::atomic<float>* crushBits = nullptr;
        std::atomic<float>* crushSR = nullptr;
        std::atomic<float>* crushOS = nullptr;

        std::atomic<float>* delayTime = nullptr;
        std::atomic<float>* delayFB = nullptr;
        std::atomic<float>* delayTone = nullptr;
        std::atomic<float>* delayPP = nullptr;

        std::atomic<float>* grainSize = nullptr;
        std::atomic<float>* grainOvl = nullptr;
        std::atomic<float>* grainPitch = nullptr;
        std::atomic<float>* grainRand = nullptr;
        std::atomic<float>* grainSpray = nullptr;
        std::atomic<float>* grainTime = nullptr;
        std::atomic<float>* grainFB = nullptr;

        std::atomic<float>* reverbSize = nullptr;
        std::atomic<float>* reverbDamp = nullptr;
        std::atomic<float>* reverbWidth = nullptr;
        std::atomic<float>* reverbPreDelay = nullptr;
    } raw;

    int pendingPresetIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveBoxAudioProcessor)
};
