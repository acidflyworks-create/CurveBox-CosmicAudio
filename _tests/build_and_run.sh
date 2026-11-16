#!/usr/bin/env bash
# Builds and runs the offline DSP smoke test (no GUI / host required).
# Usage: JUCE_DIR=/path/to/JUCE ./build_and_run.sh
set -e
JUCE_DIR="${JUCE_DIR:-/home/user/_juce}"
CXX="${CXX:-g++}"
INC="-I$JUCE_DIR/modules -I../Source -I/usr/include/freetype2"
DEFS="-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1
 -DJUCE_MODULE_AVAILABLE_juce_core=1 -DJUCE_MODULE_AVAILABLE_juce_events=1
 -DJUCE_MODULE_AVAILABLE_juce_graphics=1 -DJUCE_MODULE_AVAILABLE_juce_data_structures=1
 -DJUCE_MODULE_AVAILABLE_juce_audio_basics=1
 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0"
FLAGS="-std=c++17 -O1 $INC $DEFS"

build () { # $1 source, $2 object
  if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then $CXX $FLAGS -c "$1" -o "$2"; fi
}

build "$JUCE_DIR/modules/juce_core/juce_core.cpp" juce_core.o
build "$JUCE_DIR/modules/juce_core/juce_core_CompilationTime.cpp" juce_time.o
build "$JUCE_DIR/modules/juce_events/juce_events.cpp" juce_events.o
build "$JUCE_DIR/modules/juce_graphics/juce_graphics.cpp" juce_graphics.o
build "$JUCE_DIR/modules/juce_graphics/juce_graphics_Harfbuzz.cpp" juce_hb.o
$CXX $FLAGS -x c -c "$JUCE_DIR/modules/juce_graphics/juce_graphics_Sheenbidi.c" -o juce_sb.o

$CXX $FLAGS TestDSP.cpp juce_core.o juce_time.o juce_events.o juce_graphics.o juce_hb.o juce_sb.o \
     -o test_dsp -lpthread -ldl -lfreetype -lharfbuzz -lfontconfig
./test_dsp
