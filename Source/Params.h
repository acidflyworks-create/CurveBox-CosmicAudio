/*
    CurveBox -- Params.h
    Shared parameter identifiers, module description tables and tempo division
    helpers. Header-only, no state.
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>

namespace cb
{

//==============================================================================
enum Module
{
    Volume = 0,
    Filter,
    Drive,
    Crush,
    Pan,
    Delay,
    Grain,
    Reverb,
    NumModules
};

static constexpr int kNumModules = (int) NumModules;

/*  Amount parameter is called "Depth" for the insert shapers and "Send" for the
    three send effects, exactly as described in the design brief.
*/
struct ModuleInfo
{
    const char* prefix;     // parameter id prefix, e.g. "vol"
    const char* name;       // display name
    const char* amountName; // "Depth" / "Send"
    juce::uint32 colour;    // planet accent colour
    bool isSend;
};

inline const std::array<ModuleInfo, kNumModules>& moduleInfo()
{
    static const std::array<ModuleInfo, kNumModules> info { {
        { "vol", "Volume", "Depth", 0xff4a4a4a, false },   // 0xff4a4a4a is replaced below
        { "flt", "Filter", "Depth", 0xff4a4a4a, false },
        { "drv", "Drive",  "Depth", 0xff4a4a4a, false },
        { "crs", "Crush",  "Depth", 0xff4a4a4a, false },
        { "pan", "Pan",    "Depth", 0xff4a4a4a, false },
        { "dly", "Delay",  "Send",  0xff4a4a4a, true  },
        { "grn", "Grain",  "Send",  0xff4a4a4a, true  },
        { "rev", "Reverb", "Send",  0xff4a4a4a, true  }
    } };

    return info;
}

// Actual accent colours (kept separate so the table above stays readable).
inline juce::Colour moduleColour (int module)
{
    static const std::array<juce::Colour, kNumModules> colours { {
        juce::Colour (0xffffc94a), // Volume  -- yellow star
        juce::Colour (0xff5fc8ff), // Filter  -- ice blue gas giant
        juce::Colour (0xffff6a3d), // Drive   -- lava orange
        juce::Colour (0xff9d8bff), // Crush   -- violet moon
        juce::Colour (0xff4dffc3), // Pan     -- teal
        juce::Colour (0xff6f9bff), // Delay   -- deep blue
        juce::Colour (0xffe07bff), // Grain   -- magenta dust
        juce::Colour (0xff8fd7ff)  // Reverb  -- pale cyan atmosphere
    } };

    return colours[(size_t) juce::jlimit (0, kNumModules - 1, module)];
}

inline juce::String modulePrefix (int module)
{
    return juce::String (moduleInfo()[(size_t) juce::jlimit (0, kNumModules - 1, module)].prefix);
}

inline juce::String moduleName (int module)
{
    return juce::String (moduleInfo()[(size_t) juce::jlimit (0, kNumModules - 1, module)].name);
}

inline juce::String moduleAmountName (int module)
{
    return juce::String (moduleInfo()[(size_t) juce::jlimit (0, kNumModules - 1, module)].amountName);
}

inline bool isSendModule (int module)
{
    return moduleInfo()[(size_t) juce::jlimit (0, kNumModules - 1, module)].isSend;
}

//==============================================================================
// Parameter ids
//==============================================================================
inline juce::String pid (int module, const juce::String& suffix)
{
    return modulePrefix (module) + "_" + suffix;
}

namespace ids
{
    static const juce::String mix     { "glob_mix" };
    static const juce::String output  { "glob_output" };
    static const juce::String smooth  { "glob_smooth" };
    static const juce::String offset  { "glob_offset" };
    static const juce::String midiRetrig { "glob_midi_retrig" };

    // Per module
    static const juce::String enabled { "en" };
    static const juce::String amount  { "amt" };
    static const juce::String rate    { "rate" };

    // Filter
    static const juce::String filterMode { "mode" };
    static const juce::String filterFreq { "freq" };
    static const juce::String filterRes  { "res" };

    // Drive
    static const juce::String driveTone { "tone" };
    static const juce::String driveOS   { "os" };

    // Crush
    static const juce::String crushBits  { "bits" };
    static const juce::String crushSR    { "sr" };
    static const juce::String crushOS    { "os" };

    // Delay
    static const juce::String delayTime { "time" };
    static const juce::String delayFB   { "fb" };
    static const juce::String delayTone { "tone" };
    static const juce::String delayPP   { "pp" };

    // Grain
    static const juce::String grainSize   { "size" };
    static const juce::String grainOvl    { "ovl" };
    static const juce::String grainPitch  { "pitch" };
    static const juce::String grainRand   { "randp" };
    static const juce::String grainSpray  { "spray" };
    static const juce::String grainTime   { "time" };
    static const juce::String grainFB     { "fb" };

    // Reverb
    static const juce::String reverbSize   { "size" };
    static const juce::String reverbDamp   { "damp" };
    static const juce::String reverbWidth  { "width" };
    static const juce::String reverbPreDelay { "predly" };
}

//==============================================================================
// Tempo divisions
//==============================================================================
inline const juce::StringArray& rateNames()
{
    static const juce::StringArray names { "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4",
                                           "1/8", "1/16", "1/32", "1/2T", "1/4T", "1/8T", "1/16T" };
    return names;
}

/** Cycle length in beats for each entry of rateNames() (4/4 assumed). */
inline const std::array<double, 12>& rateBeats()
{
    static const std::array<double, 12> beats { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5,
                                                0.25, 0.125,
                                                4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return beats;
}

inline const juce::StringArray& delayTimeNames()
{
    static const juce::StringArray names { "1/2", "1/4", "1/4.", "1/4T", "1/8",
                                           "1/8.", "1/8T", "1/16", "1/16.", "1/16T" };
    return names;
}

inline const std::array<double, 10>& delayTimeBeats()
{
    static const std::array<double, 10> beats { 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5,
                                                0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0 };
    return beats;
}

inline const juce::StringArray& filterModeNames()
{
    static const juce::StringArray names { "Low Pass", "High Pass", "Band Pass" };
    return names;
}

/** Default rate index per module ("1/4" -> index 4). */
inline int defaultRateIndex (int module)
{
    switch (module)
    {
        case Volume: return 4;   // 1/4
        case Filter: return 5;   // 1/8
        case Drive:  return 4;
        case Crush:  return 5;
        case Pan:    return 4;
        case Delay:  return 4;
        case Grain:  return 2;   // 1 bar
        case Reverb: return 2;
        default:     return 4;
    }
}

/** Index of "1/4" inside delayTimeNames(). */
static constexpr int kDefaultDelayTimeIndex = 1;

} // namespace cb
