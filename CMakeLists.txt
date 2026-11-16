cmake_minimum_required(VERSION 3.22)

# 'C' is required by JUCE 8 (it compiles a couple of C sources: sheenbidi, ...).
project(CurveBox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CURVEBOX_COPY_PLUGIN_AFTER_BUILD
       "Copy the built VST3 into the system VST3 folder after every build" OFF)

# Which formats to build, e.g. -DCURVEBOX_BUILD_FORMATS="VST3" for a fast iteration.
set(CURVEBOX_BUILD_FORMATS "VST3" CACHE STRING "Formats to build (VST3, VST, Standalone, AU)")
set(CURVEBOX_VST2_SDK_PATH "" CACHE PATH "Path to a licensed VST2 SDK root")
option(CURVEBOX_BUILD_TESTS "Build offline DSP smoke tests" OFF)
separate_arguments(CURVEBOX_FORMAT_LIST UNIX_COMMAND "${CURVEBOX_BUILD_FORMATS}")

# ------------------------------------------------------------------------------
# JUCE via FetchContent
# ------------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

if("VST" IN_LIST CURVEBOX_FORMAT_LIST)
    if(NOT EXISTS "${CURVEBOX_VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h")
        message(FATAL_ERROR "VST2 requires a licensed SDK. Set CURVEBOX_VST2_SDK_PATH to the SDK root containing pluginterfaces/vst2.x/aeffect.h.")
    endif()
    juce_set_vst2_sdk_path("${CURVEBOX_VST2_SDK_PATH}")
endif()

# ------------------------------------------------------------------------------
# Plugin target
# ------------------------------------------------------------------------------
juce_add_plugin(CurveBox
    COMPANY_NAME              "CurveBox Audio"
    BUNDLE_ID                 "com.curveboxaudio.curvebox"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   ${CURVEBOX_COPY_PLUGIN_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE  Cvbx
    PLUGIN_CODE               Crbx
    FORMATS                   ${CURVEBOX_FORMAT_LIST}
    PRODUCT_NAME              "CurveBox"
    VST3_CATEGORIES           "Fx" "Distortion"
    AU_MAIN_TYPE              "kAudioUnitType_Effect"
    AUMID                     "CurveBox"
    VST3_AUTO_MANIFEST        TRUE
)

target_sources(CurveBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Only the files listed above exist as translation units; every other header is
# header-only on purpose (Curve.h, Effects.h, Params.h, Presets.h, CurveEditor.h,
# SpaceLookAndFeel.h, PlanetView.h).
target_include_directories(CurveBox PRIVATE Source)

target_compile_definitions(CurveBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(CurveBox
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------------------
# Compiler specific tweaks
# ------------------------------------------------------------------------------
if(MSVC)
    # /utf-8  : sources are UTF-8, avoids C4819 "character cannot be represented"
    # /MP     : compile the big JUCE translation units in parallel
    # /bigobj : juce_audio_processors generates a very large object file
    target_compile_options(CurveBox PRIVATE /utf-8 /MP /bigobj)
endif()

if(APPLE)
    # Universal binary (Apple Silicon + Intel): -DCURVEBOX_MACOS_UNIVERSAL=ON
    option(CURVEBOX_MACOS_UNIVERSAL "Build a universal macOS binary" OFF)
    if(CURVEBOX_MACOS_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()

if(CURVEBOX_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(CurveBoxDSPTests PRODUCT_NAME "CurveBoxDSPTests")
    target_sources(CurveBoxDSPTests PRIVATE _tests/TestDSP.cpp)
    target_include_directories(CurveBoxDSPTests PRIVATE Source)
    target_compile_definitions(CurveBoxDSPTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(CurveBoxDSPTests PRIVATE juce::juce_graphics)
    if(MSVC)
        target_compile_options(CurveBoxDSPTests PRIVATE /utf-8 /bigobj)
    endif()
    add_test(NAME CurveBoxDSP COMMAND CurveBoxDSPTests)
endif()
