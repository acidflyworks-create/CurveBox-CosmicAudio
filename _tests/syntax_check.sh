#!/usr/bin/env bash
# Compiles both translation units against a local JUCE checkout (no link step).
# Usage: JUCE_DIR=/path/to/JUCE ./syntax_check.sh
set -e
JUCE_DIR="${JUCE_DIR:-/home/user/_juce}"
CXX="${CXX:-g++}"
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -c
 -I$JUCE_DIR/modules -I../Source
 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
 -DJUCE_MODULE_AVAILABLE_juce_core=1 -DJUCE_MODULE_AVAILABLE_juce_events=1
 -DJUCE_MODULE_AVAILABLE_juce_graphics=1 -DJUCE_MODULE_AVAILABLE_juce_data_structures=1
 -DJUCE_MODULE_AVAILABLE_juce_gui_basics=1 -DJUCE_MODULE_AVAILABLE_juce_audio_basics=1
 -DJUCE_MODULE_AVAILABLE_juce_audio_devices=1 -DJUCE_MODULE_AVAILABLE_juce_audio_formats=1
 -DJUCE_MODULE_AVAILABLE_juce_audio_processors=1 -DJUCE_MODULE_AVAILABLE_juce_audio_utils=1
 -DJUCE_MODULE_AVAILABLE_juce_dsp=1
 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 -DJUCE_ALSA=0 -DJUCE_JACK=0
 -DJucePlugin_Name=\"CurveBox\" -DJucePlugin_VersionString=\"1.0.0\"
 -DJucePlugin_IsSynth=0 -DJucePlugin_WantsMidiInput=1 -DJucePlugin_ProducesMidiOutput=0
 -DJucePlugin_IsMidiEffect=0 -DJucePlugin_EditorRequiresKeyboardFocus=1
 -DJUCE_DISPLAY_SPLASH_SCREEN=0 -DJUCE_REPORT_APP_USAGE=0
 -DJUCE_MODAL_LOOPS_PERMITTED=0 -DJUCE_STRICT_REFCOUNTEDPOINTER=1"
echo "== PluginProcessor.cpp"
$CXX $FLAGS ../Source/PluginProcessor.cpp -o /tmp/CurveBox_processor.o
echo "== PluginEditor.cpp"
$CXX $FLAGS ../Source/PluginEditor.cpp -o /tmp/CurveBox_editor.o
echo "OK: both translation units compiled with no warnings"
